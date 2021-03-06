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

struct vector_clock{
	int motion_sensor;
	int key_chain;
	int door;
	int gateway;
	//bool isSensor;
	//int sockid;
	//int lastValue;
};

struct vector_clock vc;
int arr1[10],arr2[10];
char GatewayPort[7];
char vector_msg[100];
char GatewayIP[30],Sensor[50],SensorPort[7],SensorArea[5],SensorIP[16]; 
pthread_t pthread;
int clnt;
int arrcount=0;
int clnt,master_socket;
bool socketCreated=false;
FILE *output;
int client_socket[30];

int count=0;

struct sensor_device{
	int id;
	char IP[16];
	char Port[7];
	int sockid;
	char type[10];
};

struct sensor_device connectionList[20];
int connectionCount = 0;

void device_register();
void set_vector_clock();
int MakeConnection();
void *ReadParameters(void *filename);
void ReadConfig(char *filename);
void init_vector_clock();
void* otherSensors(void * client_socket);
void* SetConnection();
int connectToSensor(char *, char *);
void writeToFile();
void parse_vectorclock(char *);


int main(int argc, char *argv[])
{
	int i=0,j=0;
	int k=0;
	int client_socket;
	init_vector_clock();
	printf("\n");
	ReadConfig(argv[1]);
	clnt=MakeConnection();
	device_register(clnt);
	client_socket=clnt;

	if(argc<3)
	{
		printf("Incorrect Arguments!\nFormat: .\\a.out <Door Configuration File> <Door State File> <Door Output File>\n");
		exit(0);
	}

	output=fopen(argv[3],"w+");

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

	fscanf(config,"%[^,],%s\ndoor,%[^,],%s",GatewayIP,GatewayPort,SensorIP,SensorPort);

	fclose(config);

}


void init_vector_clock()
{
	int i=0;
	vc.motion_sensor=0;
	vc.key_chain=0;
	vc.door=0;
	vc.gateway=0;
}

//Function to send temperature to gateway at specific intervals
void *ReadParameters(void *filename)
{
	FILE *param;
	int curr_ts = 0, end_time = 0,last_end=0;
	int start_time;	
	char Message[100],value[100];
	char temp[10];
	int k;
	int sndmsg=0;
	int flag=0,isFirstTime=1; 
	int message_size;
	char client_message[256];
	char *file=(char*)filename;
	char end[5];
	char *vc_msg;
	param=fopen(file,"r");		
	printf("\n");
	sleep(10);
	while(1)
	{	
			//last_end=end_time;
			//printf("%d\n", last_end);
			fscanf(param,"%[^;];%s\n",end,value);
			int end_time=atoi(end);
			//printf("%d %d\n",curr_ts,end_time );
			if(feof(param))
			{
				fseek(param, 0, SEEK_SET);
				curr_ts = 0;
				end_time = 0;
				last_end=0;
			}
			bzero(Message,100);
			//send the message only if the value is changed from open to close
			if((flag==1 && strcmp(value,"Open")==0 && isFirstTime==0)|| (flag==0 && strcmp(value,"Close")==0)) //THe already sent value is "Open" so no need to send it again
			{
				sndmsg=0;
			}
			else if(flag==0 && isFirstTime==1)//send the 1st value to gateway irrespective of whatever it is			
			{
		          flag=1;
		          sndmsg=1;
		          isFirstTime=0;
			}
			else if(flag==0 && isFirstTime==0 && strcmp(value,"Open")==0)//if previous value sent was "close" and new value is "open", then report the state change
			{
				//isFirstTime=0;
				flag=1;
				sndmsg=1;
				//sndmsg=1;
			}
			else if(flag==1 && isFirstTime==0 && strcmp(value,"Close")==0)//if previous value sent was "open" and new value is "close", then report the state change
			{
				flag=0;
				sndmsg=1;
			}
			
			if(sndmsg==1)  //send message only if the conditions are satisfied
			{	
			//	printf("%d %d\n",end_time,last_end);	
				sleep(end_time-last_end);
				sprintf(temp,"%s",value);
				set_vector_clock();
				sprintf(Message, "door-%s-%u-%s-%s-%s",temp,(unsigned)time(NULL),vector_msg,SensorIP,SensorPort);//,vc_msg);
				printf("%s\n",Message );
				for(k=0;k<connectionCount;k++)
				{
					send(connectionList[k].sockid,Message,strlen(Message),0);
				}
				fprintf(output,"%s\n",Message );
				sndmsg=0;
				strcpy(Message,"");
			
			}
				last_end=end_time;	
			
	//	}
	}
}


//Function to register device to the Gateway
void device_register(int clnt)
{

	char Message[MESSAGE_LENGTH];
	sprintf(Message,"Type:register;Action:door-%s-%s",SensorIP,SensorPort);

	//printf("Register Message to Gateway : %s\n",Message);
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
	printf("Door Registered: %s\n", Message);
}


//function to set the vector clock
void set_vector_clock()
{
	//char* vector_msg;
	vc.door++;
    sprintf(vector_msg,"[%d,%d,%d,%d]",vc.motion_sensor,vc.key_chain,vc.door,vc.gateway);
    //return vector_msg;

}

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

void* SetConnection()
{
	
    int addrlen , new_socket , max_clients = 30 , activity, i , valread , sd;
    int max_sd;
    struct sockaddr_in address;
      
    char buffer[1025];  //data buffer of 1K
      
    //set of socket descriptors
    fd_set readfds;
    
	//initialize Socket
	int userThread = 0;
	int yes=1,c,clnt,k;
	int temp_sockfd;
	int GPort;

	for (i = 0; i < max_clients; i++) 
    {
        client_socket[i] = 0;
    }
    
	master_socket = socket(AF_INET,SOCK_STREAM,0);
	//Socket Creation

	if(master_socket < 0)
	{
		perror("socket");
		exit(1);
	}	

	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(atoi(SensorPort));	

	if(setsockopt(master_socket,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(yes))==-1)
	{
		perror("setsockopt");
		close(master_socket);
		exit(1);
	}

	//Bind the name to the socket
	if(bind(master_socket,(struct sockaddr*)&address,sizeof(address))<0)
	{
		perror("bind");
		close(master_socket);
		exit(1);
	}
	
	//Listen at the particular socket for connection requests
	listen(master_socket,10);
	
	//Accept incoming messages and spawn a thread for each incoming connection at the socket
	
	addrlen = sizeof(address);
     
    while(1) 
    {
        //clear the socket set
        FD_ZERO(&readfds);
  
        //add master socket to set
        FD_SET(master_socket, &readfds);
        max_sd = master_socket;
         
        
        //add child sockets to set
        for ( i = 0 ; i < max_clients ; i++) 
        {
            //socket descriptor
            sd = client_socket[i];
             
            //if valid socket descriptor then add to read list
            if(sd > 0)
                FD_SET( sd , &readfds);
             
            //highest file descriptor number, need it for the select function
            if(sd > max_sd)
                max_sd = sd;
        }

        //wait for an activity on one of the sockets , timeout is NULL , so wait indefinitely
        activity = select( max_sd + 1 , &readfds , NULL , NULL , NULL);

        if ((activity < 0) && (errno!=EINTR)) 
        {
            printf("select error");
        }
          
        //If something happened on the master socket , then its an incoming connection
        if (FD_ISSET(master_socket, &readfds)) 
        {
            if ((new_socket = accept(master_socket, (struct sockaddr *)&address, (socklen_t*)&addrlen))<0)
            {
                perror("accept");
                exit(EXIT_FAILURE);
            }
            //inform user of socket number - used in send and receive commands

			connectionList[connectionCount].sockid=new_socket;
			connectionList[connectionCount].id=connectionCount+1;
			connectionCount++;     

            //add new socket to array of sockets
            for (i = 0; i < max_clients; i++) 
            {
                //if position is empty
                if( client_socket[i] == 0 )
                {
                    client_socket[i] = new_socket;
                    break;
                }
            }
        }
          
        //else its some IO operation on some other socket :)
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


int connectToSensor(char *IP, char* Port)
{
	char Message[256];
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
    //printf("New connection , socket fd is %d , ip is : %s , port : %d \n" , cl , inet_ntoa(sock.sin_addr) , ntohs(sock.sin_port));
	sprintf(Message,"Register:door,%s,%s",SensorIP,SensorPort);

	send(cl,Message,strlen(Message),0);
	return cl;
}

void writeToFile(int sock)
{
		char data[256]="";
		char device[100],action[100],ts[100],vc[100],ip[100],port[100];
		int mess_size,k;
		if((mess_size = recv(sock,data,sizeof(data),0))<0)
		{
			perror("recv");
			exit(1);
		}
		//printf("%s",data);
		//fprintf(output,"%s\n",data);
	
		sscanf(data,"%[^-]-%[^-]-%[^-]-%[^-]-%[^-]-%s",device,action,ts,vc,ip,port);
		if(strstr(vc,"["))
			parse_vectorclock(vc);
		//fprintf(output,"%s\n",data);
		fflush(output);
}


void parse_vectorclock(char vectorm[100])
{

	int i=0,j=0,k=0,l=0,vcms,vckc,vcd;
	char motion_sensor_val[100],key_chain_val[100],door_val[100];
	i++;
	//printf("\n in parse vector function  %s \n",vectorm);
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
	//printf("\nThe values are: %s %s  %s",motion_sensor_val,key_chain_val,door_val);

	if(vcms>vc.motion_sensor)
		vc.motion_sensor=vcms;
	if(vckc>vc.key_chain)
		vc.key_chain=vckc;
	//vc.motion_sensor=atoi(motion_sensor_val);
	//vc.
}
