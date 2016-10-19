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

#define MAX_CONNECTION 20
#define MESSAGE_LENGTH 256
#define MAX_SENSORS 3

struct sensor_device{
	int id;
	char IP[16];
	char Port[7];
	bool isSensor;
	int sockid;
	char type[20];
	char lastvalue[10];
	char secondLastValue[10];
};

int countid=0;
char register_message[50];
bool sig = false;
struct sensor_device connectionList[MAX_CONNECTION];
int connectionCount = 0; 
char GatewayPort[6];
char GatewayIP[20];
FILE *output;
pthread_t thread, thread_setValue;
int master_socket;
int max_sensor=0;
FILE *devStatus;
char scan_type[20],scan_status[20],scan_ts[20],scan_vector[20],scan_IP[20],scan_port[20];
char database[500];
int doorFlag=0;
int motionFlag=0;
int keychain;
int motion;
int keychainFlag=0;
int systemFlag = 0;
void readCall(int );
void *setValue();
void  INThandler(int sig);
void ReadConfig(char *filename);
void MakeConnection();

int main(int argc, char *argv[])
{

	int clnt;

	if(argc<3)
	{
		printf("Incorrect Arguments!\nFormat: .\\a.out <Gateway Configuration Path> <Gateway Output File>\n");
		exit(0);
	}

	output=fopen(argv[2],"w+");
	
	signal(SIGINT, INThandler); 
	signal(SIGTSTP, INThandler); 
	
	ReadConfig(argv[1]);
	
	MakeConnection();
	fclose(output);
		
	return 0;
}

void readCall(int sd)
{
	pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
	int message_size,i;
	char message[100]="\0";
	char status[4];
	char message_type[10];
	char action[100];
	char type_temp[20],insert_message[256];
	int value,j,k;
	char fmessage[100];
	char temp_area[3];

	/*if(connectionCount==5 && sendFlag==0)
	{
		for(i=0;i<connectionCount;i++)
		{
			sendFlag=1;
			if(strcmp(connectionList[i].type,"database")!=0)
				if(strcmp(connectionList[i].type,"securitydevice")!=0)
					write(connectionList[i].sockid,"Start\n",strlen("Start\n"));
		}
	}
	*/

	strcpy(message,"\0");
	if((message_size = read(sd,message,100))<0)
	{
		perror("recv");
		exit(0);
	}

	if(message_size != 0)
	{
		memset(action,0,100);
		fprintf(output,"%s\n", message);
		fflush(output);
		if(strstr(message,"register"))
		{
			sscanf(message, "Type:%[^;];Action:%s", message_type,action);
			if(strcmp("register", message_type) == 0)
			{
				sscanf(action, "%[^-]-%[^-]-%s", type_temp, connectionList[connectionCount].IP, connectionList[connectionCount].Port);
				connectionList[connectionCount].sockid = sd;
				connectionList[connectionCount].id = connectionCount+1;;
				strcpy(connectionList[connectionCount].type,type_temp);
				if((strcmp(type_temp, "motion") == 0) || (strcmp(type_temp, "door") == 0) || (strcmp(type_temp, "keychain") == 0))
				{
					max_sensor++;
					connectionList[connectionCount].isSensor = true;
				}
				else
				{
					connectionList[connectionCount].isSensor = false; 
				}
				connectionCount++;
				if(max_sensor==3)
				{
					for(k=0;k<connectionCount;k++)
					{
						if(strcmp(connectionList[k].type,"database")!=0)
						if(strcmp(connectionList[k].type,"securitydevice")!=0)
						for(i=0;i<connectionCount;i++)
						{
							if(strcmp(connectionList[i].type,"database")!=0)
							if(strcmp(connectionList[i].type,"securitydevice")!=0)
							{
								sprintf(register_message,"Register:%s,%s,%s\n",connectionList[i].type,connectionList[i].IP,connectionList[i].Port);
								write(connectionList[k].sockid,register_message,strlen(register_message));
								strcpy(register_message,"\0");
							}
						}
					}
				}
			}
			

		}
		else
		{
			

			sscanf(message,"%[^-]-%[^-]-%[^-]-%[^-]-%[^-]-%s",scan_type,scan_status,scan_ts,scan_vector,scan_IP,scan_port);
			//printf("%s,%s,%s,%s,%s,%s\n",scan_type,scan_status,scan_ts,scan_vector,scan_IP,scan_port);
//			printf("%s %s\n",scan_status,scan_type );
			if(strcmp(scan_status,"Close")==0)
				doorFlag=1;
			if(doorFlag==1)
			{
				
				if(strcmp(scan_type,"motion")==0)
				{
					motionFlag=1;
//					printf("Incerment motion\n");
				}
				if(strcmp(scan_type,"keychain")==0)
				{
					keychainFlag=1;
//					printf("Incerment keychain\n");
				}
			}
			if(keychainFlag == 1 && motionFlag == 1)
			{
				//printf("motionFlag %d\n",motionFlag);
				for(i=0;i<connectionCount;i++)
				{
					if(strcmp(connectionList[i].type,"motion")==0)
					{
						motion=i;
				//		printf("Motion Sock Got %d\n",motion);
					}
				
					if(strcmp(connectionList[i].type,"keychain")==0)
					{
						keychain=i;
				//		printf("keychain Sock Got %d\n",keychain);
					}

				}
				if((strcmp(connectionList[motion].lastvalue,"True")==0) && (strcmp(connectionList[keychain].lastvalue,"True")==0) && systemFlag==1)
				{
					printf("Motion Sensor - %s, Keychain Sensor - %s",connectionList[motion].lastvalue,connectionList[keychain].lastvalue );
					printf("\tUser Back home!\n");
					for(i=0;i<connectionCount;i++)
					{
						if(strcmp(connectionList[i].type,"securitydevice")==0)
						{
							write(connectionList[i].sockid,"Type:Switch;Action:OFF\n",strlen("Type:Switch;Action:OFF\0"));
							fprintf(output,"Type:Switch;Action:OFF\n");
							fflush(output);
						}
					}
					systemFlag=0;
				}

				if((strcmp(connectionList[motion].lastvalue,"True")==0) && (strcmp(connectionList[keychain].lastvalue,"False")==0)  && systemFlag==1)
				{
					printf("Motion Sensor - %s, Keychain Sensor - %s",connectionList[motion].lastvalue,connectionList[keychain].lastvalue );
					printf("\tIntruder Alarm\n");
					for(i=0;i<connectionCount;i++)
					{
						if(strcmp(connectionList[i].type,"securitydevice")==0)
						{
							write(connectionList[i].sockid,"Type:IntruderAlarm;Action:ON\n",strlen("Type:IntruderAlarm;Action:ON\0"));
							fprintf(output,"Type:IntruderAlarm;Action:ON\n");
							fflush(output);
						}
					}

				}

				if((strcmp(connectionList[motion].lastvalue,"False")==0) && (strcmp(connectionList[keychain].lastvalue,"False")==0))
				{
					printf("Motion Sensor - %s, Keychain Sensor - %s",connectionList[motion].lastvalue,connectionList[keychain].lastvalue );
					printf("\tUser out of home!\n");
					for(i=0;i<connectionCount;i++)
					{
						if(strcmp(connectionList[i].type,"securitydevice")==0)
						{
							write(connectionList[i].sockid,"Type:Switch;Action:ON\n",strlen("Type:Switch;Action:ON\0"));		
							fprintf(output,"Type:Switch;Action:ON\n");
							fflush(output);
						}
					}
					systemFlag=1;
				}
				
				doorFlag=0;
				motionFlag=0;
				keychainFlag=0;
			}

			for(j=0;j<connectionCount;j++)
				if(sd==connectionList[j].sockid)
					break;
			for(k=0;k<connectionCount;k++)
			{
				if(connectionList[k].sockid==sd)
				{
					strcpy(connectionList[k].lastvalue,scan_status);
					//printf("Changing Last value for %s %s \n",connectionList[k].type,connectionList[k].lastvalue );
				}


				if(strcmp(connectionList[k].type,"database")==0)
				{	
					memset(insert_message,0,256);
					sprintf(insert_message,"insert:%d----%s----%s----%s----%s----%s\n",connectionList[j].id,scan_type,scan_status,scan_ts,scan_IP,scan_port);
					write(connectionList[k].sockid,insert_message,strlen(insert_message));
					//printf("%s",insert_message);
					strcpy(insert_message,"");
				}
			}

		}
		
	}

}

//Function to handle Cntrl + C and Cntrl + z
void  INThandler(int sig)
{
	sig = true;
    signal(sig, SIG_IGN);
	pthread_detach(thread);
	pthread_detach(thread_setValue);
	kill(getpid(),SIGKILL);

}

//Function to read Gateway Config File
void ReadConfig(char *filename)
{
	FILE *config;
	config=fopen(filename,"r");
	fscanf(config,"%[^:]:%s\n",GatewayIP,GatewayPort);
	fclose(config);					
}


//Function to accept incoming messages and spawn a thread
void MakeConnection()
{
	
    int addrlen , new_socket , client_socket[30] , max_clients = 30 , activity, i , valread , sd;
    int max_sd;
    struct sockaddr_in address;
      
    char buffer[1025];  //data buffer of 1K
      
    //set of socket descriptors
    fd_set readfds;
    
	//initialize Socket
	int userThread = 0;
	int yes=1,c,clnt;
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
	address.sin_port = htons(atoi(GatewayPort));	

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
	
	//Listen at the particular socket for connection requests
	listen(master_socket,10);

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
			countid++;
            if ((new_socket = accept(master_socket, (struct sockaddr *)&address, (socklen_t*)&addrlen))<0)
            {
                perror("accept");
                exit(EXIT_FAILURE);
            }
              
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
          
       if(countid>=5)
       {
	        for (i = 0; i < max_clients; i++) 
	        {
	            sd = client_socket[i];
	              
	           if (FD_ISSET( sd , &readfds)) 
	            {
	            	readCall(sd);		  
			    }
			}
		}
	}
	fclose(output);
	close(master_socket);
				
}
