#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<netdb.h>
#include<signal.h>
#include<fcntl.h>

#define CLIENTMAX 1000
#define BYTES 1024

char *ROOT;
int listenfd, clients[CLIENTMAX];
void error(char *);
void helping();
void startServer(char *);
void respond(void *);
void createThread(int);

pthread_t ntid[5]; 
pthread_mutex_t lock[5];

int main(int argc, char* argv[])
{
    struct sockaddr_in clientaddr;
    socklen_t addrlen;
    char c;
    int tmp=0;
    while(tmp<5){
    	pthread_mutex_init(&lock[tmp],NULL);
    	pthread_mutex_lock(&lock[tmp]);
    	createThread(tmp);
    	tmp++;
    }
    //дефолтные значения
    char PORT[6];
    ROOT = getenv("PWD");//возвращает значение переменной окружения
    strcpy(PORT,"10000"); //копирование из второй строки в первую. возвращает указатель на результирующую строку

    int slot=0;

    //парсим аргументы
    while ((c = getopt (argc, argv, "p:r:h")) != -1)
        switch (c)
        {
            case 'r':
                ROOT = malloc(strlen(optarg));//распределяет байты в памяти и возвращает указатель на память. strlen - длина строки, optarg - строковое значение параметра
                strcpy(ROOT,optarg);
                break;
            case 'p':
                strcpy(PORT,optarg);
                break;
	    case 'h':
		helping();
            case '?':
                fprintf(stderr,"Wrong arguments given!!!\n");
                exit(1);
            default:
                exit(1);
        }
    
    //установка всех элементов в -1
    int i;
    for (i=0; i<CLIENTMAX; i++)
        clients[i]=-1;
    startServer(PORT);
    printf("Server started at port no. %s%s%s with root directory as %s%s%s\n","\033[92m",PORT,"\033[0m","\033[92m",ROOT,"\033[0m");
    int j=0;
    // прием соединений
    while (1)
    {
        addrlen = sizeof(clientaddr);
        clients[slot] = accept (listenfd, (struct sockaddr *) &clientaddr, &addrlen);//принять соединение на сокете (сокет, адрес другой стороны, длина адреса в байтах)
        if(j>4)
		{
			j=0;
            int k=0;
            int coeff=slot/5;
            while(k<5)
			{
				if(pthread_mutex_trylock(&lock[k])==0)
					createThread(k+5*coeff);
                k++;
			}
		}
        if (clients[slot]<0)
            error ("accept() error");
        else
        {
                pthread_mutex_unlock(&lock[j]);        
        }
		j++;
        while (clients[slot]!=-1) slot = (slot+1)%CLIENTMAX;
    }
    return 0;
}
//помощь
void helping()
{
	printf("Desired options:\n -p port\n -r directory contains requested files\n -h help\n");
	exit(0);
}
//запуск сервера
void startServer(char *port)
{
    struct addrinfo hints, *res, *p;//идентифицирует хост, сервер

    // getaddrinfo 
    memset (&hints, 0, sizeof(hints));//заполняет память первого аргумента символами (второй аргумент) 
    hints.ai_family = AF_INET;//область в которой происходит работа(TCP/IP)
    hints.ai_socktype = SOCK_STREAM;//тип - посылаются потоки байтов
    hints.ai_flags = AI_PASSIVE;//не указывается сетевой адрес каждой структуры
    if (getaddrinfo( NULL, port, &hints, &res) != 0)//преобразует сетевой адрес и сервис. возвращает адреса сокетов. устанавливает значение res для указания на выделяемый динамический список структур addrinfo - hints (предпочтительный тип сокета или протокол). port - номер порта в сетевом адресе
    {
        perror ("getaddrinfo() error");
        exit(1);
    }
    // сокет
    for (p = res; p!=NULL; p=p->ai_next)
    {
        listenfd = socket (p->ai_family, p->ai_socktype, 0);//создает конечную точку соединения. первый аргумент - набор протоколов, для создания соединения. второй аргумент - тип сокета. третий аргумент - конкретный протокол, который работает с сокетом (0 - один протокол)
        if (listenfd == -1) continue;// если вернул ошибку - -1. иначе, возвращает описатель, ссылающийся на сокет
        if (bind(listenfd, p->ai_addr, p->ai_addrlen) == 0) break;//привязка имени к сокету. сокет - первый аргумент, второй аргумент - локальный адрес, третий - длина. возвращается 0 в случае успеха
    }
    if (p==NULL)
    {
        perror ("socket() or bind()");
        exit(1);
    }

    freeaddrinfo(res);//освобождает память, предназначенную для списка res

    if ( listen (listenfd, 1000000) != 0 )//слушать соединение на сокете. в случае успеха возвращается 0
    {
        perror("listen() error");
        exit(1);
    }
}

//соединение клиента
void respond(void *arg)
{
	char mesg[99999], *reqline[3], data_to_send[BYTES], path[99999];
    int *p=(int *)arg;
    int n=*p;
    int rcvd, fd, bytes_read;
    memset( (void*)mesg, (int)'\0', 99999 );//заполняет память первого аргумента символами (второй аргумент) 
    pthread_mutex_lock(&lock[n%5]);
    
    rcvd=recv(clients[n], mesg, 99999, 0);//получить сообщение из сокета, если соединение установлено. возвращает количество принятых байт. записывает во второй аргумент

    if (rcvd<0)    // ошибка получения
        fprintf(stderr,("recv() error\n"));
    else if (rcvd==0)  
        fprintf(stderr,"Client disconnected upexpectedly.\n");
    else    // сообщение получения
    {
        printf("%s", mesg);
        reqline[0] = strtok (mesg, " \t\n");//извлечение токенов из строки (непустых строк, состоящих из символов, не встречающихся во второй строке
        if ( strncmp(reqline[0], "GET\0", 4)==0 )//сравнивает первые 4 символа 2 строк. возвращает 0 в случае равенства
        {
            reqline[1] = strtok (NULL, " \t");
            reqline[2] = strtok (NULL, " \t\n");
            if ( strncmp( reqline[2], "HTTP/1.0", 8)!=0 && strncmp( reqline[2], "HTTP/1.1", 8)!=0 )//строки не совпадают 
            {
                write(clients[n], "HTTP/1.0 400 Bad Request\n", 25);
            }
            else
            {
                if ( strncmp(reqline[1], "/\0", 2)==0 )
                    reqline[1] = "/index.html";        //если клиент не запросил файл, откроется файл по умолчанию

                strcpy(path, ROOT);//копирование строки из второго аргумента в первый
                strcpy(&path[strlen(ROOT)], reqline[1]);
                printf("file: %s\n", path);

                if ( (fd=open(path, O_RDONLY))!=-1 )    //открытие файла
                {
                    send(clients[n], "HTTP/1.0 200 OK\n\n", 17, 0);//отправляет сообщение в сокет
                    while ( (bytes_read=read(fd, data_to_send, BYTES))>0 )//запись файлового описателя fd в буфер
                        write (clients[n], data_to_send, bytes_read);//запись в описатель файла
                }
                else    write(clients[n], "HTTP/1.0 404 Not Found\n", 23); //файл не найден
            }
        }pthread_mutex_unlock(&lock[n%5]);
    }

    //закрытие сокета
    shutdown (clients[n], SHUT_RDWR);         
    close(clients[n]);//закрытие файлового дескриптора
    clients[n]=-1;
}
//отправка потока на выполнение
void createThread(int k){
    int *m=(int *)malloc(sizeof(int));
    *m=k;
    int err = pthread_create(&ntid,NULL,respond,(void *) m);
    if(err!=0){
        printf("error create thread");
    }
}
