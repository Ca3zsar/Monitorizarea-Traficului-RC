#include <arpa/inet.h>
#include <asm-generic/socket.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

const int PORT = 4201;
const char *database = "users.db";
socklen_t sockLength = (socklen_t)sizeof(struct sockaddr_in);
extern int errno;

fd_set activeFD;
fd_set readFD;

int keepRunning = 1;

void printError(char *message) {
  perror(message);
  exit(errno);
}

typedef struct Thread {
  int threadId;
  int clientId;
  int logged;
  struct sockaddr_in *client;
} Thread;

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
struct sockaddr_in initialize_server() {
  struct sockaddr_in server;

  bzero(&server, sizeof(server));

  server.sin_family = AF_INET;
  server.sin_addr.s_addr = htonl(INADDR_ANY);
  server.sin_port = htons(PORT);

  return server;
}

void initialize_db(sqlite3 *db) {
  int status;
  status = sqlite3_open(database, &db);

  if (status != SQLITE_OK) {
    sqlite3_close(db);
    printError("Cannot open the database!");
  }
}

void set_socket(int *socketD) {
  if ((*socketD = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
    printError("Error at creating the socket");
  }
  int opt = 1;
  setsockopt(*socketD, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
  // fcntl(*socketD, F_SETFL, O_NONBLOCK);
}

int interact(Thread *thread) {
  int msgLength;
  int numberRead;
  if ((numberRead = read(thread->clientId, &msgLength, sizeof(int))) <= 0) {
    if (numberRead < 0)
      perror("Read() error in thread");
    else
      printf("Client %d disconnected \n", thread->clientId);
    return 0;
  }

  char *message;
  message = (char *)malloc(msgLength + 1);

  if (read(thread->clientId, message, msgLength) < 0) {
    perror("Error at reading of the message in thread");
    return 0;
  }
  message[msgLength] = '\0';

  int newLength = strlen(message);
  if (write(thread->clientId, &newLength, sizeof(int)) <= 0)
    perror("Error at write in thread");

  printf("Sent back to the client: %d\n", newLength);
  return 1;
}

int login(Thread *thread)
{
  sqlite3_stmt *result;
  

  return 0;
}

void lobby(Thread *thread) {
  pthread_detach(pthread_self());

  printf("[Thread %d] - Waiting for authentication: \n", thread->threadId);
  fflush(stdout);

  if(!login(thread))
    FD_CLR(thread->clientId, &activeFD);
    close(thread->clientId);

    close((intptr_t)thread);
    return;

  while (keepRunning) {
    int answer = interact(thread);
    if (!answer)
      break;
  }
  close((intptr_t)thread);
}

void stopHandler() {
  printf("Closing the server now...\n");
  keepRunning = 0;
}

int main() {
  struct sockaddr_in serverStruct = initialize_server();
  struct sockaddr_in clientStruct;

  int socketD;
  pthread_t threads[100];

  struct timeval outTime;
  int fd;
  int fdNumber;

  set_socket(&socketD);

  sqlite3 *db;
  initialize_db(db);

  // Bind the socket.
  if (bind(socketD, (struct sockaddr *)&serverStruct,
           sizeof(struct sockaddr)) == -1)
    printError("Error at binding the socket");

  // Make the server listen for the clients.
  if (listen(socketD, 5) == -1)
    printError("[Server] Error at listen()");

  FD_ZERO(&activeFD);
  FD_SET(socketD,&activeFD);

  outTime.tv_sec = 1;
  outTime.tv_usec = 0;

  fdNumber = socketD;

  int index = 0;

  printf("[Server] Waiting at port %d \n", PORT);
  // Wait for the clients to connect and then serve them.
  while (keepRunning) {
    // See if the server was closed
    signal(SIGINT, stopHandler);
    if (!keepRunning)
      break;

    int clientId;
    Thread *currThread;

    bcopy((char*)&activeFD,(char*)&readFD,sizeof(readFD));

    if(select(fdNumber + 1,&readFD,NULL,NULL,&outTime) < 0)
      printError("[Server] Error at select()\n");
    
    if(FD_ISSET(socketD, &readFD))
    {
      bzero(&clientStruct,sockLength);
      clientId = accept(socketD,(struct sockaddr*)&clientStruct,&sockLength);

      if(clientId < 0){
        perror("[Server] Error at accepting client");
        continue;
      }

      if(fdNumber < clientId)
        fdNumber = clientId;

      FD_SET(clientId,&activeFD);

      currThread = (struct Thread *)malloc(sizeof(struct Thread));
      bcopy((struct sockaddr*)&clientStruct,(struct sockaddr*)&currThread->client,sockLength);
      currThread->threadId = index++;
      currThread->clientId = clientId;

      pthread_create(&threads[index % 100], NULL, (void *)&lobby, currThread);
    }

    // // Accept a client in blocking way.
    // if ((clientId = accept(socketD, (struct sockaddr *)&clientStruct,
    //                        &sockLength)) < 0) {
    //   perror("[Server]Accept error\n");
    //   continue;
    // }

  }
  return 0;
}
