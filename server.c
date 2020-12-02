#include <arpa/inet.h>
#include <asm-generic/socket.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

const int PORT = 4200;
socklen_t sockLength = (socklen_t)sizeof(struct sockaddr_in); 
extern int errno;

void printError(char *message)
{
    perror(message);
    exit(errno);
}

typedef struct Thread{
  int threadId; 
  int clientId;
}Thread;

// Struct to describe client.
struct {
  char *username;
  char *password;
  char subscribed;
  float actualSpeed;
  float coordinates[2];
} clientInfo;

// Struct to describe information from the traffic
struct {
  float speedLimit;
  float coordinates[2];
} trafficInfo;

// Struct to describe an accident
struct {
  char *address;
  char *message;
  float coordinates[2];
} trafficEvents;

// Struct to describe the newsletter. (For the subscribed clients only).
struct {
  char *weather;
  char *sport;
  float prices[3];
} news;

// Initialize a server structure.
struct sockaddr_in initialize_server()
{
  struct sockaddr_in server;

  bzero(&server, sizeof(server));
  
  server.sin_family = AF_INET;
  server.sin_addr.s_addr = htonl(INADDR_ANY);
  server.sin_port = htons(PORT);

  return server;
}

void set_socket(int *socketD)
{
  if((*socketD = socket(AF_INET,SOCK_STREAM,0)) == -1)
  {
    printError("Error at creating the socket");
  }
  setsockopt(*socketD, SOL_SOCKET, SO_REUSEADDR, (const void*)1, sizeof(int));
}

int interact(Thread *thread)
{
  int msgLength;
  if(read(thread->clientId,&msgLength,sizeof(int)) <= 0){
    perror("Read() error in thread");
    return 0;
  }
  
  char *message;
  message = (char*)malloc(msgLength+1);

  if(read(thread->clientId,message,msgLength) <= 0){
    perror("Error at reading of the message in thread");
    return 0;
  }
  message[msgLength]='\0';

  int newLength = strlen(message);
  if(write(thread->clientId,&newLength,sizeof(int)) <= 0)
    perror("Error at write in thread");

  printf("Sent back to the client: %d\n",newLength);
  return 1;
}

void lobby(Thread *thread)
{
  printf("[Thread %d] - Wait for the message: \n",thread->threadId);
  
  fflush(stdout);
  pthread_detach(pthread_self());
  while(1)
  {
    int answer = interact(thread);
    if(!answer)break;
  }
  close((intptr_t)thread);
}



int main() {
  struct sockaddr_in serverStruct = initialize_server();  
  struct sockaddr_in clientStruct;

  int socketD;
  pthread_t threads[100];

  set_socket(&socketD);

  //Bind the socket.
  if(bind(socketD,(struct sockaddr*)&serverStruct,sizeof(struct sockaddr)) == -1)
    printError("Error at binding the socket");
  
  //Make the server listen for the clients.
  if(listen(socketD,5) == -1)
    printError("[Server] Error at listen()");

  int index = 0;

  //Wait for the clients to connect and then serve them.
  while(1)
  {
    int clientId;
    Thread *currThread;
    
    printf("[Server] Waiting at port %d \n",PORT);
    //Accept a client in blocking way.
    if(( clientId = accept(socketD,(struct sockaddr*) &clientStruct,&sockLength)) < 0)
    {
        perror("[Server]Accept error\n");
        continue;
    }

    currThread = (struct Thread*)malloc(sizeof(struct Thread));
    currThread->threadId = index++ ;
    currThread->clientId = clientId;

    pthread_create(&threads[index],NULL,(void*)&lobby, currThread);
  }

  return 0; 
}
