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

static fd_set activeFD;
static fd_set readFD;
static fd_set writeFD;

sqlite3 *db;

pthread_mutex_t mlock=PTHREAD_MUTEX_INITIALIZER;  

static int volatile fdNumber;
int keepRunning = 1;


typedef struct Thread {
  int threadId;
  int clientId;
  int logged;
  pthread_t thread;
} Thread;

// Struct to describe client.
struct clientInfo {
  char *username;
  char subscribed;
  float actualSpeed;
  float coordinates[2];
};

// Struct to describe information from the traffic
struct trafficInfo {
  float speedLimit;
  float coordinates[2];
};

// Struct to describe an accident
struct trafficEvents {
  char *address;
  char *message;
  float coordinates[2];
};

// Struct to describe the newsletter. (For the subscribed clients only).
struct news {
  char *weather;
  char *sport;
  float prices[3];
};


void printError(char *message) {
  perror(message);
  exit(errno);
}

int write_to_client(int threadId, int socketD, char *message) {
  int msgLength = strlen(message);
  send(socketD, &msgLength, sizeof(int), 0);

  int written;
  if ((written = (send(socketD, message, msgLength, 0))) != msgLength) {
    printf("[Thread %d] Error at writing to client %d", threadId, socketD);
    return 0;
  }

  return 1;
}

char *read_from_client(int threadId, int socketD) {
  int readLength;
  char *message;

  int status;

  if ((status=read(socketD, &readLength, sizeof(int))) <= 0) {
    if(status == 0)printf("[Thread %d]Client %d disconnected\n.",threadId,socketD);
    else printf("[Thread %d] Error at reading length of message from client %d",
           threadId, socketD);
    return 0;
  }

  message = (char *)malloc(readLength + 1);

  if ((status=read(socketD, message, readLength)) != readLength) {
        if(status == 0)printf("[Thread %d]Client %d disconnected\n.",threadId,socketD);
        else printf("[Thread %d] Error at reading message from client %d", threadId,
           socketD);
    return 0;
  }
  // if(message[readLength-1]=='\n')message[readLength-1]='\0';
  message[readLength] = '\0';
  return message;
}

// Initialize a server structure.
struct sockaddr_in initialize_server() {
  struct sockaddr_in server;

  bzero(&server, sizeof(server));

  server.sin_family = AF_INET;
  server.sin_addr.s_addr = htonl(INADDR_ANY);
  server.sin_port = htons(PORT);

  return server;
}

struct args {
  int socketD;
  int clientId;
  int threadId;
  struct sockaddr_in clientStruct;
  struct clientInfo client;
};


void initialize_db() {
  int status;
  status = sqlite3_open(database, &db);

  if (status != SQLITE_OK) {
    sqlite3_close(db);
    printError("Cannot open the database!");
  }
}

int correct_user(char *username, char *password, struct clientInfo *client) {
  // char *password;
  sqlite3_stmt *result;
  char *sql = "SELECT password, subscribed FROM Users WHERE name =?";

  int status = sqlite3_prepare_v2(db, sql, -1, &result, 0);
  if (status == SQLITE_OK) {
    sqlite3_bind_text(result, 1, username, strlen(username), NULL);
  } else {
    printf("Failed to execute SQL query\n");
    return 0;
  }

  int res;
  int step = sqlite3_step(result);
  if (step == SQLITE_ROW) {
    client->username = (char*)malloc(strlen(username)+1);
    sprintf(client->username,"%s",username);
    client->subscribed = sqlite3_column_int(result,1);
    res = strcmp((char *)sqlite3_column_text(result, 0), password);
    sqlite3_finalize(result);
    return !res;
  }
  sqlite3_finalize(result);

  return 0;
}

int is_in_database(char *username)
{
  sqlite3_stmt *result;
  char *sql = "SELECT COUNT(*) FROM Users WHERE name =?";

  int status = sqlite3_prepare_v2(db,sql,-1,&result,0);
  if (status == SQLITE_OK) {
    sqlite3_bind_text(result, 1, username, strlen(username), NULL);
  } else {
    printf("Failed to execute SQL query\n");
    return 0;
  }

  int step = sqlite3_step(result);
  int res;
  if (step == SQLITE_ROW) {
    res = sqlite3_column_int(result, 0);
    sqlite3_finalize(result);
    return res;
  }
  sqlite3_finalize(result);
  return 0;
}

void set_socket(int *socketD) {
  if ((*socketD = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
    printError("Error at creating the socket");
  }
  int opt = 1;
  setsockopt(*socketD, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
  // fcntl(*socketD, F_SETFL, O_NONBLOCK);
}

static int login(struct args *arg) {
  int tries = 3;

  char *question = "Enter your username: ";
  char *askPass = "Enter your password: ";
  int status;

  while (tries > 0) {
    if (!write_to_client(arg->threadId, arg->clientId, question))
      return 0;
    char *username;

    if (!(username = read_from_client(arg->threadId, arg->clientId)))
      return 0;
    int corect;

    if (!write_to_client(arg->threadId, arg->clientId, askPass))
      return 0;

    char *userPass;
    if (!(userPass = read_from_client(arg->threadId, arg->clientId)))
      return 0;

    if (correct_user(username, userPass,&arg->client)) {
      corect = 1;
      write(arg->clientId, &corect,
            sizeof(int)); // Tell that the name was correct.
      write_to_client(arg->threadId, arg->clientId, "You are now logged in, ");
      return 1;
    } else {
      corect = 0;
      write(arg->clientId, &corect, sizeof(int));

      char *incorrect = "Incorrect username or password\n";
      write_to_client(arg->threadId,arg->clientId, incorrect);
      tries--;
    }
  }
  write_to_client(arg->threadId, arg->clientId, "Number of tries exceeded\n");
  return 0;
}

static int registerNew(struct args *arg) { 
  char *question = "Enter your username: ";
  char *askPass = "Enter your password: ";
  char *askSub = "Do you want to subscribe? (0/1)";
  int status;

  int notRegistered = 1;
  while(notRegistered)
  {
    if(!write_to_client(arg->threadId, arg->socketD, question))
      return 0;
    
    char *username;

    if (!(username = read_from_client(arg->threadId, arg->clientId)))
      return 0;
    
    

  }

  return 1; }

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
  if (message[0] == '\n')
    return 1;

  int newLength = strlen(message);
  if (write(thread->clientId, &newLength, sizeof(int)) <= 0)
    perror("Error at write in thread");

  printf("Sent back to the client: %d\n", newLength);
  return 1;
}

int validate(struct args *arg) {
  char *question = "Do you want to log in or register? [L/R]";

  if (!write_to_client(arg->threadId, arg->clientId, question))
    return 0;

  char answer;
  int readLength;

  if ((readLength = read(arg->clientId, &answer, sizeof(char))) <= 0) {
    if (readLength < 0)
      printf("[Thread %d] Error at reading login option from client %d\n",
             arg->threadId, arg->clientId);
    else
      printf("Client %d disconnected \n", arg->clientId);
    return 0;
  }

  printf("[Thread %d] Client %d chose to %s\n", arg->threadId, arg->clientId,
         answer == 'l' ? "login" : "register");

  if (answer == 'l') {
    if (!login(arg)){
      return 0;
    }
    else{
      printf("[Thread %d] Client %d logged in.\n",arg->threadId,arg->clientId);
      fflush(stdout);
    }
  } else {
    if (!registerNew(arg))
      return 0;
  }
  return 1;
}

static void lobby(Thread *thread) {
  pthread_detach(pthread_self());

  fflush(stdout);

  while (keepRunning) {
    int answer = interact(thread);
    if (!answer)
      break;
  }
  FD_CLR(thread->clientId, &activeFD);
  close(thread->clientId);
  pthread_exit(NULL);
}

void stopHandler() {
  printf("Closing the server now...\n");
  keepRunning = 0;
}

void add_new_client(struct args *arg) {
  pthread_detach(pthread_self());
  bzero(&arg->clientStruct, sockLength);
  arg->clientId =
      accept(arg->socketD, (struct sockaddr *)&arg->clientStruct, &sockLength);

  printf("[Thread %d] Authenticating client %d\n", arg->threadId,
         arg->clientId);
  fflush(stdout);

  int opts = fcntl(arg->clientId, F_GETFL);
  fcntl(arg->clientId, F_SETFL, opts & O_NONBLOCK & ~O_NONBLOCK);

  if (arg->clientId < 0) {
    perror("[Server] Error at accepting client\n");
    return;
  }

  if (!validate(arg)){
    close(arg->clientId);
    return;
  }

  pthread_mutex_lock(&mlock);
  if (fdNumber < arg->clientId)
    fdNumber = arg->clientId;

  FD_SET(arg->clientId, &activeFD);
  FD_SET(arg->clientId, &readFD);
  FD_SET(arg->clientId, &writeFD);
  pthread_mutex_unlock(&mlock);
  
  pthread_exit(NULL);
}

int main() {
  struct sockaddr_in serverStruct = initialize_server();
  struct sockaddr_in clientStruct;

  int socketD;
  Thread threads[100];
  struct args arg[100];

  struct timeval outTime;
  int fd;

  set_socket(&socketD);

  initialize_db();

  // Bind the socket.
  if (bind(socketD, (struct sockaddr *)&serverStruct,
           sizeof(struct sockaddr)) == -1)
    printError("Error at binding the socket");

  // Make the server listen for the clients.
  if (listen(socketD, 5) == -1)
    printError("[Server] Error at listen()");

  FD_ZERO(&activeFD);
  FD_ZERO(&readFD);
  FD_ZERO(&writeFD);
  FD_SET(socketD, &activeFD);

  outTime.tv_sec = 1;
  outTime.tv_usec = 0;

  fdNumber = socketD;

  int index = 0;
  printf("[Server] Waiting at port %d \n", PORT);
  int first=1;
  // Wait for the clients to connect and then serve them.
  while (keepRunning) {
    // See if the server was closed
    signal(SIGINT, stopHandler);
    if (!keepRunning)
      break;

    int clientId;
    bzero(&readFD,sizeof(readFD));
    bcopy((char *)&activeFD, (char *)&readFD, sizeof(readFD));
    bcopy((char *)&activeFD, (char *)&writeFD, sizeof(writeFD));

    if (select(fdNumber + 1, &readFD, &writeFD, NULL, &outTime) < 0)
      printError("[Server] Error at select()\n");

    if (FD_ISSET(socketD, &readFD)) {
      bzero((struct args *)&arg[index], sizeof(struct args));
      arg[index % 100].socketD = socketD;
      arg[index % 100].threadId = index % 100;
      pthread_create(&threads[index % 100].thread, NULL, (void *)&add_new_client,
                     &arg[index % 100]);
      index++;
    }

    for (fd = 4; fd <= fdNumber; fd++) /* parcurgem multimea de descriptori */
    {
      // printf("%d - ",fd);
      if (fd != socketD && (FD_ISSET(fd, &readFD) || FD_ISSET(fd, &writeFD))) {
        threads[index].threadId = index;
        threads[index].clientId = fd;
        index++;
        pthread_create(&threads[(index - 1) % 100].thread, NULL, (void *)&lobby,
                       &threads[(index-1)%100]);
        FD_CLR(fd,&activeFD);
      }
    }
  }
  close(socketD);
  return 0;
}
