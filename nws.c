/************ Netlab Web Server ************/

/* Project Ergasthrio Diktywn - 2001 */


#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <signal.h>
#include <time.h>
#include <netdb.h>
#include <arpa/inet.h>


#define MAX_WORD_SIZE 100
#define NUM_OF_PARAMETERS 4

struct {
	int maxchild;
	int port;
	char blockip[MAX_WORD_SIZE];
	char documentroot[MAX_WORD_SIZE];
	char log[MAX_WORD_SIZE];
} enviro;

int sock_d, new_sock_d;	/* socket descriptors */
struct sockaddr_in serv_addr;	/* server's address */
struct sockaddr_in cl_addr;	/* client's address */
int active_children = 0, child_num = -1;
int yes = 1;
char hostname[MAX_WORD_SIZE];

void error(int);
void init(int argument_number,char *arguments[]);
void sendfile(char *, long);
void write_to_log(char *);
void terminate();
void restart();
void timeout();
void childended();

void main(int argc,char *argv[])
{
	int sin_size, pid, bytes;
	time_t t;
	struct hostent *cl;
	struct stat filestat;
	char buf[1024];	/* buffer 1K */
	char *ans, tempword[MAX_WORD_SIZE], filename[MAX_WORD_SIZE], logline[MAX_WORD_SIZE];


	init(argc,argv);

	sock_d = socket(AF_INET, SOCK_STREAM, 0);
	if ( sock_d == -1 )
		error(5);

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(enviro.port);
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	memset(&(serv_addr.sin_zero), '\0', 8);

	if (setsockopt(sock_d, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
		error(10);

	if ( bind(sock_d, (struct sockaddr *)&serv_addr, sizeof(struct sockaddr)) == -1 )
		error(6);

	sin_size = sizeof(struct sockaddr_in);

	if ( listen(sock_d, 10) == -1 )
		error(7);

	if ( signal(SIGCHLD, childended) == SIG_ERR )
		error(8);
	if ( signal(SIGINT, terminate) == SIG_ERR )
		error(8);
	if ( signal(SIGHUP, restart) == SIG_ERR )
		error(8);
	if ( signal(SIGALRM, timeout) == SIG_ERR )
		error(8);


	if ( fork() ) exit(EXIT_SUCCESS); /* ola kala - mpainei sto background */

	for (;;)
	{
		while ( active_children >= enviro.maxchild );

		while ( (new_sock_d = accept(sock_d, (struct sockaddr *)&cl_addr, &sin_size)) == -1 );

		++active_children;
		if ( ++child_num == enviro.maxchild )
			child_num = 0;

		pid = fork();

		if ( pid != 0 )
		{
			close(new_sock_d);
			continue;
		};

		/* o parakatw kwdikas ekteleitai apo to paidi */

		close(sock_d);

		strcpy(tempword,inet_ntoa(cl_addr.sin_addr));
		if ( strcmp(tempword, enviro.blockip) == 0 )
		{
			close(new_sock_d);
			exit(EXIT_SUCCESS);
		}

		alarm(20); /* h aithsh prepei na ginei mesa se 20 deyterolepta */

		bytes = recv(new_sock_d, buf, 1024, 0);
		buf[bytes+1] = NULL;

		/* mhdenizoume to xronometro */
		alarm(0);


		if ( (ans = strstr(buf, "GET")) == NULL )
		{
			close(new_sock_d);
			exit(EXIT_SUCCESS);
		}

		ans+=3;	/* dei3e thn le3h meta to GET */
		sscanf(ans,"%s",tempword);	/* pare to onoma toy arxeioy */

		if ( strcmp(tempword, "/") == 0 )
			strcpy(tempword, "/index.htm");

		if ( strstr(tempword, "../") != NULL )
			strcpy(tempword, "error.htm");

		strcat(strcpy(filename, enviro.documentroot), tempword);

		if ( stat(filename, &filestat) == -1 )
		{
			strcpy(tempword, "error.htm");
			strcat(strcpy(filename, enviro.documentroot), tempword);
			if ( stat(filename, &filestat) == -1) error(12);
		};

		cl = gethostbyaddr((char *)&cl_addr.sin_addr.s_addr, sizeof(cl_addr.sin_addr.s_addr), AF_INET);
		if ( cl == NULL )
		{
			strcpy(hostname, inet_ntoa(cl_addr.sin_addr));
		} else {
			strcpy(hostname, cl->h_name);
		}

		sendfile(filename, filestat.st_size);

		strcpy(filename, tempword); /* krata mono to requested onoma */

		t = time(0);
		ctime_r(&t, tempword, 26); /* 26 xarakthres apanthsh epistrefei h ctime() */
		tempword[24] = '\0';

		sprintf(logline, "%s %d [%s] GET %s %d bytes", hostname, child_num, tempword, filename, filestat.st_size);

		write_to_log(logline);

		close(new_sock_d);
		exit(EXIT_SUCCESS);
	};



}




void init(int arg_num,char *args[])
{
	int i = 0, port_set = 0, root_set =0, counter;
	FILE *file_pointer;
	char c,tempword[MAX_WORD_SIZE], logline[MAX_WORD_SIZE];
	time_t t;

	/* 8ese to log file */
	strcpy(enviro.log, "/u3/ceidst97/barelas/nws/nws.log");

	/* elexgos gia ta command line arguments */
	switch(arg_num)
	{
		case 1 :
			break;	/* den yparxoun command line arguments */
		case 3 :
			++i;
			break;
		case 5 :
			i = 2;
			break;
		default :	/* la8os ari8mos command line arguments */
			error(3);
			break;
	};
				
	for (counter = 1; counter <= i; ++counter)
	{
		if ( 0 == strcmp(args[2*counter-1],"-p") )
			{
			enviro.port = atoi(args[2*counter]);
			++port_set;
			continue;
			}
		else if ( 0 == strcmp(args[2*counter-1],"-d") )
			{
			strcpy(enviro.documentroot,args[2*counter]);
			++root_set;
			continue;
			}
		else
			error(4);

	};




	file_pointer = fopen("nws.conf","r");

	if (file_pointer == NULL)
		error(1);

	/* to arxeio anoi3e kanonika */

	do
	{
		c = fscanf(file_pointer,"%s",tempword);
		if ( c != EOF )
		{
			if ( 0 == strcmp(tempword,"MaxChild") )
			{
				c = fscanf(file_pointer,"%i",&enviro.maxchild);
				++i;
				continue;
			};
			if ( (0 == strcmp(tempword,"Port")) && (!(port_set)) )
			{
				c = fscanf(file_pointer,"%i",&enviro.port);
				++i;
				continue;
			};
			if ( 0 == strcmp(tempword,"BlockIP") )
			{
				c = fscanf(file_pointer,"%s",enviro.blockip);
				++i;
				continue;
			};
			if ( (0 == strcmp(tempword,"DocumentRoot")) && (!(root_set)) )
			{
				c = fscanf(file_pointer,"%s",enviro.documentroot);
				++i;
				continue;
			};
		};
	}
	while (c != EOF);

	if (i != NUM_OF_PARAMETERS)	/* exoun oristei oles oi parametroi? */
		error(2);

	t = time(0);
	ctime_r(&t, tempword, 26); /* 26 xarakthres apanthsh epistrefei h ctime() */
	tempword[24] = '\0';
	sprintf(logline, "  -  P  [%s]  Starting server  -", tempword);
	write_to_log(logline);

	fclose(file_pointer);	/* ola kala - kleisimo arxeioy configuration */

	
}

void error(int error_num)
{
	switch(error_num)
	{
		case 1 :
			printf("\ncannot open configuration file - server terminated\n");
			break;
		case 2 :
			printf("\nparameter problem - server terminated\n");
			break;
		case 3 :
			printf("\ncannot run server - wrong number of command line arguments\n");
			printf("Usage: nwserver [-p <Port_number>] [-d <Document_Root>]\n");
			break;
		case 4 :
			printf("\ncannot run server - wrong command line argument\n");
			printf("Usage: nwserver [-p <Port_number>] [-d <Document_Root>]\n");
			break;
		case 5 :
			perror("nws - socket");
			break;
		case 6 :
			perror("nws - bind");
			break;
		case 7 :
			perror("nws - listen");
			break;
		case 8 :
			perror("nws - signal");
			break;
		case 9 :
			perror("nws - nws.log missing");
			break;
		case 10 :
			perror("nws - setsockopt");
			break;
		case 11 :
			perror("nws - exec");
			break;
		case 12 :
			perror("nws - error.htm missing");
			break;
		default :
			printf("\nwrong error number - internal error - server terminated\n");
			break;
	};
	exit(EXIT_FAILURE);
}

void sendfile(char *filename, long fs)
{
	FILE *fp;
	int bytes_send, counter = 0;
	char c, tempword[MAX_WORD_SIZE], mes[1024];
	long i;

	sprintf(mes, "HTTP/1.0 200 Document follows\nMIME-Version: 1.0\nServer: NWS\nContent-Type: text/html\nContent-Length: %d\n\n", fs);
	fp = fopen(filename, "r");
	if (fp == NULL)
		bytes_send = send(new_sock_d, "file not found",14,0);
	else
	{

		for (i = strlen(filename)-3; counter <= 3; counter++)
			tempword[counter] = filename[i+counter];

		if ( 0 == strcasecmp(tempword, "gif") )
			sprintf(mes, "HTTP/1.0 200 Document follows\nMIME-Version: 1.0\nServer: NWS\nContent-Type: image/gif\nContent-Length: %d\n\n", fs);
		if ( 0 == strcasecmp(tempword, "jpg") )
			sprintf(mes, "HTTP/1.0 200 Document follows\nMIME-Version: 1.0\nServer: NWS\nContent-Type: image/jpeg\nContent-Length: %d\n\n", fs);
		if ( 0 == strcasecmp(tempword, "txt") )
			sprintf(mes, "HTTP/1.0 200 Document follows\nMIME-Version: 1.0\nServer: NWS\nContent-Type: text/plain\nContent-Length: %d\n\n", fs);

		bytes_send = send(new_sock_d, mes, strlen(mes), 0);

		for (i=0; i < fs; i++)
		{
			c = fgetc(fp);
			bytes_send = send(new_sock_d, &c, 1, 0);
		}

		fclose(fp);
	};



}

void write_to_log(char *s)
{
	FILE *fp;
	fp = fopen(enviro.log, "a");
	if ( fp == NULL )
		error(9);
	fprintf(fp, "%s\n", s);

	fclose(fp);
}

void terminate()
{
	char logline[MAX_WORD_SIZE], tempword[MAX_WORD_SIZE];
	time_t t;

	close(sock_d); /* kleinei th syndesh */

	t = time(0);
	ctime_r(&t, tempword, 26); /* 26 xarakthres apanthsh epistrefei h ctime() */
	tempword[24] = '\0';

	sprintf(logline, "INT(OS_signal)  P  [%s]  Terminating server  -", tempword);

	write_to_log(logline);

	exit(EXIT_SUCCESS);
}

void restart()
{
	char logline[MAX_WORD_SIZE], tempword[MAX_WORD_SIZE];
	time_t t;

	close(sock_d); /* kleinei th syndesh */

	t = time(0);
	ctime_r(&t, tempword, 26); /* 26 xarakthres apanthsh epistrefei h ctime() */
	tempword[24] = '\0';

	sprintf(logline, "HUP(OS_signal)  P  [%s]  Restarting server  -", tempword);

	write_to_log(logline);

	if (execlp("nwserver", "nwserver", NULL) == -1) /* epanekkinhsh */
		error(11);
	
}

void timeout()
{
	char logline[MAX_WORD_SIZE], tempword[MAX_WORD_SIZE];
	time_t t;
	struct hostent *cl;

	cl = gethostbyaddr((char *)&cl_addr.sin_addr.s_addr, sizeof(cl_addr.sin_addr.s_addr), AF_INET);

	t = time(0);
	ctime_r(&t, tempword, 26); /* 26 xarakthres apanthsh epistrefei h ctime() */
	tempword[24] = '\0';

	sprintf(logline, "%s %d [%s]  Timed out  -", hostname, child_num, tempword);

	write_to_log(logline);

	exit(EXIT_SUCCESS);
}

void childended()
{
	int i;
	if ( signal(SIGCHLD, childended) == SIG_ERR )
		error(8);
	wait(&i);
	--active_children;
}