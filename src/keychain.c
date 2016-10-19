#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/types.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>   
#include <arpa/inet.h>  
#include <sys/time.h>

#define MESSAGE_LENGTH 256

char GatewayPort[7];
char GatewayIP[30],KeyChainPort[7],KeyChainIP[16]; 
pthread_t pthread;
int clnt,master_socket;
bool socketCreated=false;
FILE *output;
int client_socket[30];
int count=0;

void device_register();
void* otherSensors(void * client_socket);
int MakeConnection();
void *ReadParameters(void *filename);
void ReadConfig(char *filename);
void* SetConnection();
int connectToSensor(char *, char *);
void writeToFile();
void set_vector_clock();
void init_vector_clock();
void parse_vectorclock(char *);


struct vector_clock{
	int motion_sensor;
	int key_chain;
	int door;
	int gateway;
};

struct vector_clock vc;
char vector_msg[100];
struct sensor_device{
	int id;
	char IP[16];
	char Port[7];
	int sockid;
	char type[10];
};

struct sensor_device connectionList[20];
int connectionCount = 0;

int main(int argc, char *argv[])
{
	int i=0,j=0;
	int k=0;
	int client_socket;

	printf("\n");
	
	init_vector_clock();
	
	ReadConfig(argv[1]);
	
	clnt=MakeConnection();
	
	device_register(clnt);

	if(argc<3)
	{
		printf("Incorrect Arguments!\nFormat: .\\a.out <KeyChain Configuration File> <KeyChain State File> <KeyChain Output File>\n");
		exit(0);
	}

	client_socket=clnt;

	output=fopen(argv[3],"w+");	
	
	//Read the state file and send those parameters to the gateway and multicast them
	if(pthread_create(&pthread,NULL,ReadParameters,(void*)argv[2])!=0)
	{
		perror("pthread_create");
	}

	//Create a server socket to accept incoming connections
	if(pthread_create(&pthread,NULL,SetConnection,NULL)!=0)
	{
		perror("pthread_create");
	}

	//Create a client socket after gateway tells this sensor to send connection requests to other sensors
	if(pthread_create(&pthread,NULL,otherSensors,(void*)client_socket)!=0)
	{
		perror("pthread_create");
	}
	pthread_join(pthread,NULL);	
	pthread_detach(pthread);

	fclose(output);
	return 0;
}

//Function to initialise the vector
void init_vector_clock()
{
	int i=0;
	vc.motion_sensor=0;
	vc.key_chain=0;
	vc.door=0;
	vc.gateway=0;
}

//Function to set the vectoy values
void set_vector_clock()
{
	//char* vector_msg;
	vc.key_chain++;
    sprintf(vector_msg,"[%d,%d,%d,%d]",vc.motion_sensor,vc.key_chain,vc.door,vc.gateway);
    //return vector_msg;
}


//Function to connect to Gateway
int MakeConnection()
{
	int clnt,i;
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

	fscanf(config,"%[^,],%s\nkeychain,%[^,],%s",GatewayIP,GatewayPort,KeyChainIP,KeyChainPort);

	fclose(config);
}

//Function to send keychain status to gateway after 5 seconds
void *ReadParameters(void *filename)
{
	FILE *param;
	int curr_ts = 0, end_time = 0;
	int start_time;
	char value[8];	
	char Message[100];
	char temp[10];
	int k;
	int message_size;
	char client_message[256];
	char *file=(char*)filename;
	printf("\n");
	param=fopen(file,"r");			
	sleep(10);	
	while(1)
	{	
		if(curr_ts >= end_time)
		{
			fscanf(param,"%d;%d;%s\n",&start_time,&end_time,value);
			if(feof(param))
			{
				fseek(param, 0, SEEK_SET);

				curr_ts = 0;
				end_time = 0;
			}
		}
		bzero(Message,100);
		sprintf(temp,"%s",value);
		set_vector_clock();
				
		sprintf(Message, "keychain-%s-%u-%s-%s-%s",temp,(unsigned)time(NULL),vector_msg,KeyChainIP,KeyChainPort);
		for(k=0;k<connectionCount;k++)
		{

			send(connectionList[k].sockid,Message,strlen(Message),0);
			
		}
		printf("%s\n",Message);
		fprintf(output,"%s\n",Message);
		fflush(output);
		curr_ts=curr_ts+5;
		sleep(5);
		message_size=0;
	}
}


//Function to register device to the Gateway
void device_register(int clnt)
{
	int k;
	char Message[MESSAGE_LENGTH];
	sprintf(Message,"Type:register;Action:keychain-%s-%s",KeyChainIP,KeyChainPort);
	//printf("Message Sent : %s\0",Message);
	if(send(clnt,Message,strlen(Message),0)<0)
	{
		perror("send");
		printf("\nUnable to send message");
	}
	strcpy(connectionList[connectionCount].type,"gateway");
	strcpy(connectionList[connectionCount].IP,GatewayIP);
	strcpy(connectionList[connectionCount].Port,GatewayPort);
	connectionList[connectionCount].sockid=clnt;
	connectionList[connectionCount].id=connectionCount+1;
	connectionCount++;
	printf("Keychain Registered - %s", Message);
	
}

//Function to accept data from the other sensors
void* otherSensors(void *client_socket)
{
	int message_size;
	char client_message[256];
	char setValue[10];
	int k=0;
	int sockid;
	int client_socket_local=0;
	client_socket_local=(int)client_socket;

	while(1)
	{	
		if((message_size = recv(client_socket_local,client_message,sizeof(client_message),0))<0)
		{
			printf("Error in read here\n");
			perror("read");
			exit(1);
		}

		sscanf(client_message,"Register:%[^,],%[^,],%[^,]",connectionList[connectionCount].type,connectionList[connectionCount].IP,connectionList[connectionCount].Port);
		count++;
		connectionList[connectionCount].sockid=connectToSensor(connectionList[connectionCount].IP,connectionList[connectionCount].Port);
		connectionList[connectionCount].id=connectionCount+1;
		connectionCount++;	

	}	
}

//Function to create a server and listen for incoming connections from the motion and door sensor
void* SetConnection()
{
	
    int addrlen , new_socket , max_clients = 30 , activity, i , valread , sd;
    int max_sd;
    struct sockaddr_in address;
      
    char buffer[1025]; 
      
    fd_set readfds;

	int userThread = 0;
	int yes=1,c,clnt,k;
	int temp_sockfd;
	int GPort;

	for (i = 0; i < max_clients; i++) 
    {
        client_socket[i] = 0;
    }
    
	master_socket = socket(AF_INET,SOCK_STREAM,0);

	if(master_socket < 0)
	{
		perror("socket");
		exit(1);
	}	

	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(atoi(KeyChainPort));	

	if(setsockopt(master_socket,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(yes))==-1)
	{
		perror("setsockopt");
		close(master_socket);
		exit(1);
	}

	if(bind(master_socket,(struct sockaddr*)&address,sizeof(address))<0)
	{
		perror("bind");
		close(master_socket);
		exit(1);
	}
	
	listen(master_socket,10);
	
	addrlen = sizeof(address);
     
    while(1) 
    {
        FD_ZERO(&readfds);
  
        FD_SET(master_socket, &readfds);
        max_sd = master_socket;
  
        for ( i = 0 ; i < max_clients ; i++) 
        {
            sd = client_socket[i];
             
            if(sd > 0)
                FD_SET( sd , &readfds);
             
            if(sd > max_sd)
                max_sd = sd;
        }

        activity = select( max_sd + 1 , &readfds , NULL , NULL , NULL);

        if ((activity < 0) && (errno!=EINTR)) 
        {
            printf("select error");
        }
          
        if (FD_ISSET(master_socket, &readfds)) 
        {
            if ((new_socket = accept(master_socket, (struct sockaddr *)&address, (socklen_t*)&addrlen))<0)
            {
                perror("accept");
                exit(EXIT_FAILURE);
            }
            
			connectionList[connectionCount].sockid=new_socket;
			connectionList[connectionCount].id=connectionCount+1;

			connectionCount++;     

            for (i = 0; i < max_clients; i++) 
            {
                if( client_socket[i] == 0 )
                {
                    client_socket[i] = new_socket;
                    break;
                }
            }
        }
          
        for (i = 0; i < max_clients; i++) 
        {
            sd = client_socket[i];
              
           if (FD_ISSET( sd , &readfds)) 
            {
 		       	writeToFile(sd);	
 		       	break;	  
		    }
		}
	}
	close(master_socket);
				
}

//Function to create a client socket and connect to the motion and door sensors
int connectToSensor(char *IP, char* Port)
{
	char Message[50];
	int cl;
	struct sockaddr_in sock;
	
	cl=socket(AF_INET,SOCK_STREAM,0);
	if(cl<0)
	{
		perror("create");
		exit(1);
	}

	sock.sin_addr.s_addr = inet_addr(IP);
	sock.sin_family = AF_INET;
	sock.sin_port = htons(atoi(Port));

	if(connect(cl,(struct sockaddr*)&sock,sizeof(sock))<0)
	{	
		perror("connect");
		close(cl);
		exit(0);
	}
  
	sprintf(Message,"Regiser:keychain,%s,%s",KeyChainIP,KeyChainPort);

	if(send(cl,Message,strlen(Message),0) < 0)
	{
		perror("send");
		printf("\nUnable to send message");
	}

	return cl;
}

//Function to write the incoming data to a file and call the parse vector function
void writeToFile(int sock)
{
		char data[100]="";
		char device[100],action[100],ts[100],vc[100],ip[100],port[100];
	
		int mess_size,k;
		if((mess_size = recv(sock,data,sizeof(data),0))<0)
		{
			perror("recv");
			exit(1);
		}
		
		sscanf(data,"%[^-]-%[^-]-%[^-]-%[^-]-%[^-]-%s",device,action,ts,vc,ip,port);
		
		if(strstr(vc,"["))
			parse_vectorclock(vc);
}

//Function to parse the vector and update it
void parse_vectorclock(char vectorm[100])
{

	int i=0,j=0,k=0,l=0,vcms,vckc,vcd;
	char motion_sensor_val[100],key_chain_val[100],door_val[100];
	i++;
	while(vectorm[i]!=',')
	{
		motion_sensor_val[j]=vectorm[i];
		i++;
		j++;
	}
	motion_sensor_val[j]='\0';
	vcms=atoi(motion_sensor_val);
	i++;
	while(vectorm[i]!=',')
	{
		key_chain_val[k]=vectorm[i];
		i++;
		k++;
	}
	key_chain_val[k]='\0';
	vckc=atoi(key_chain_val);
	i++;
	while(vectorm[i]!=',')
	{
		door_val[l]=vectorm[i];
		l++;
		i++;
	}
	door_val[l]='\0';
	vcd=atoi(door_val);

	if(vcms>vc.motion_sensor)
		vc.motion_sensor=vcms;
	if(vcd>vc.door)
		vc.door=vcd;
}
