#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/types.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <signal.h>

#define MESSAGE_LENGTH 256

int arr1[10],arr2[10];
char GatewayPort[7];
char vector_msg[100];
char GatewayIP[30],BackendPort[7],BackendIP[16]; 
pthread_t pthread;
int clnt;
int arrcount=0;
FILE *output;

void device_register();
void* writeToFile(void *filename);
int MakeConnection();
void *ReadParameters(void *filename);
void ReadConfig(char *filename);
void logDetails(char *,char *);

int main(int argc, char *argv[])
{
	int i=0,j=0;
	int k=0;

	if(argc<2)
	{
		printf("Incorrect Arguments!\nFormat: .\\a.out <Database Configuration File> <Database Output File>\n");
		exit(0);
	}

	output=fopen(argv[2],"w+");
	
	ReadConfig(argv[1]);
	clnt=MakeConnection();
	
	device_register(clnt);
	if(pthread_create(&pthread,NULL,writeToFile,(void*)argv[2])!=0)
	{
		perror("pthread_create");
	}
	
	pthread_join(pthread,NULL);	
	pthread_detach(pthread);
	
	fclose(output);

	return 0;
}

//Function to connect to Gateway
int MakeConnection()
{
	int clnt;
	struct sockaddr_in sock;
	
	clnt=socket(AF_INET,SOCK_STREAM,0);
	if(clnt<0)
	{
		perror("create");
		exit(1);
	}

	sock.sin_addr.s_addr = inet_addr(GatewayIP);
	sock.sin_family = AF_INET;
	sock.sin_port = htons(atoi(GatewayPort));

	if(connect(clnt,(struct sockaddr*)&sock,sizeof(sock))<0)
	{	
		perror("connect");
		close(clnt);
		exit(0);
	}
	return clnt;
}

//Function to read config file
void ReadConfig(char *filename)
{
	FILE *config;
	config=fopen(filename,"r");
	fscanf(config,"%[^,],%s\nbackend,%[^,],%s",GatewayIP,GatewayPort,BackendIP,BackendPort);
	//printf("%s,%s\nbackend,%s,%s",GatewayIP,GatewayPort,BackendIP,BackendPort);
	fclose(config);					
}

void logDetails(char *m1,char *m2)
{
	FILE *config;
	char msg[100];
	config=fopen(m1,"w+");
	//strcpy(msg,"Door : ");
	//strcat(msg,m);

	fprintf(config,"\n%s\n",m2);
	fclose(config);
}

//Function to register device to the Gateway
void device_register(int clnt)
{

	char Message[MESSAGE_LENGTH];

	sprintf(Message,"Type:register;Action:database-%s,%s",BackendIP,BackendPort);

	printf("Database Registered : %s\n",Message);
	if(send(clnt,Message,strlen(Message),0)<0)
	{
		perror("send");
		printf("\nUnable to send message");
	}
}

void* writeToFile(void *filename)
{
	int message_size;
	char client_message[500];
	char message[500];
	while(1)
	{	
		memset(client_message,0,500);
		strcpy(client_message,"");
		//Read incoming messages to Gateway
		if((message_size = recv(clnt,client_message,sizeof(client_message),0))<0)
		{
			perror("read");
			exit(1);
		}
		
		sscanf(client_message,"insert:%s\n",message);
		fprintf(output,"%s\n",message);
		fflush(output);
	}	

}
