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
static int closed[100] = {[0 ... 99] = 1};

sqlite3 *db;

pthread_mutex_t mlock = PTHREAD_MUTEX_INITIALIZER;

static int volatile fdNumber;
int keepRunning = 1;

// Struct to describe client.
struct clientInfo {
  char *username;
  char subscribed;
  float actualSpeed;
  float coordinates[2];
} clients[100];

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

struct args {
  int socketD;
  int clientId;
  int threadId;
  int active;
  pthread_t thread;
  struct sockaddr_in clientStruct;
  struct clientInfo client;
};

void printError(char *message) {
  perror(message);
  exit(errno);
}

int write_to_client(int threadId, int socketD, char *message) {
  int msgLength = strlen(message);
  send(socketD, &msgLength, sizeof(int), MSG_NOSIGNAL);

  int written;
  if ((written = (send(socketD, message, msgLength, MSG_NOSIGNAL))) !=
      msgLength) {
    printf("[Thread %d] Error at writing to client %d", threadId, socketD);
    return 0;
  }

  return 1;
}

char *read_from_client(int threadId, int socketD) {
  int readLength;
  char *message;

  int status;

  if ((status = read(socketD, &readLength, sizeof(int))) <= 0) {
    if (status == 0)
      printf("[Thread %d]Client %d disconnected\n.", threadId, socketD);
    else
      printf("[Thread %d] Error at reading length of message from client %d",
             threadId, socketD);
    return 0;
  }

  message = (char *)malloc(readLength + 1);

  if ((status = read(socketD, message, readLength)) != readLength) {
    if (status == 0)
      printf("[Thread %d]Client %d disconnected.\n", threadId, socketD);
    else
      printf("[Thread %d] Error at reading message from client %d", threadId,
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

void initialize_db() {
  int status;
  status = sqlite3_open(database, &db);

  if (status != SQLITE_OK) {
    sqlite3_close(db);
    printError("Cannot open the database!");
  }
}

void append_client(int clientId, struct clientInfo newClient) {
  clients[clientId].username = (char *)malloc(strlen(newClient.username));
  sprintf(clients[clientId].username, "%s", newClient.username);

  clients[clientId].subscribed = newClient.subscribed;
}

void delete_client(int clientId) {
  bzero(&clients[clientId], sizeof(struct clientInfo));
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
    client->username = (char *)malloc(strlen(username) + 1);
    sprintf(client->username, "%s", username);
    client->subscribed = sqlite3_column_int(result, 1);
    res = strcmp((char *)sqlite3_column_text(result, 0), password);
    sqlite3_finalize(result);
    return !res;
  }
  sqlite3_finalize(result);

  return 0;
}

int is_in_database(char *username) {
  sqlite3_stmt *result;
  char *sql = "SELECT COUNT(*) FROM Users WHERE name =?";

  int status = sqlite3_prepare_v2(db, sql, -1, &result, 0);
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

int add_to_db(char *username, char *password, int subscribed) {
  char sql[100];
  char *err_msg = 0;
  int status;

  sprintf(sql, "INSERT INTO Users VALUES('%s' , '%s', '%d');", username,
          password, subscribed);

  status = sqlite3_exec(db, sql, 0, 0, &err_msg);

  if (status != SQLITE_OK) {

    fprintf(stderr, "SQL error: %s\n", err_msg);

    sqlite3_free(err_msg);
    return 0;
  }

  return 1;
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

    if (correct_user(username, userPass, &arg->client)) {
      append_client(arg->clientId, arg->client);

      corect = 1;
      write(arg->clientId, &corect,
            sizeof(int)); // Tell that the name was correct.
      write_to_client(arg->threadId, arg->clientId, "You are now logged in, ");
      return 1;
    } else {
      corect = 0;
      write(arg->clientId, &corect, sizeof(int));

      char *incorrect = "Incorrect username or password\n";
      write_to_client(arg->threadId, arg->clientId, incorrect);
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
  while (notRegistered) {
    if (!write_to_client(arg->threadId, arg->clientId, question))
      return 0;

    char *username;

    if (!(username = read_from_client(arg->threadId, arg->clientId)))
      return 0;

    if (!is_in_database(username)) {
      fflush(stdout);
      int corect = 1;
      if (!write(arg->clientId, &corect, sizeof(int)))
        return 0;

      if (!write_to_client(arg->threadId, arg->clientId, askPass))
        return 0;

      char *password;
      if (!(password = read_from_client(arg->threadId, arg->clientId)))
        return 0;

      if (!write_to_client(arg->threadId, arg->clientId, askSub))
        return 0;

      int sub;
      if (!read(arg->clientId, &sub, sizeof(int)))
        return 0;

      if (!add_to_db(username, password, sub))
        return 0;

      char *success = "Successfully registered, ";

      if (!write_to_client(arg->threadId, arg->clientId, success))
        return 0;

      printf("[Thread %d]Successfully added new user to database.\n",
             arg->threadId);
      fflush(stdout);

      return 1;
    } else {
      int corect = 0;
      if (!write(arg->clientId, &corect, sizeof(int)))
        return 0;

      char *failed = "Username already existent!\n";
      if (!write_to_client(arg->threadId, arg->clientId, failed))
        return 0;
    }
  }

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
    if (!login(arg)) {
      return 0;
    } else {
      printf("[Thread %d] Client %d logged in.\n", arg->threadId,
             arg->clientId);
      fflush(stdout);
    }
  } else if (answer == 'r') {
    if (!registerNew(arg)) {
      return 0;
    } else {
      printf("[Thread %d] Client %d registered a new account.\n", arg->threadId,
             arg->clientId);
      fflush(stdout);
    }
  }
  return 1;
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
    closed[arg->clientId] = 1;
    perror("[Server] Error at accepting client\n");
    return;
  }

  if (!validate(arg)) {
    closed[arg->clientId] = 1;
    close(arg->clientId);
    return;
  }
  closed[arg->clientId] = 0;
  pthread_mutex_lock(&mlock);
  if (fdNumber < arg->clientId)
    fdNumber = arg->clientId;

  FD_SET(arg->clientId, &activeFD);
  FD_SET(arg->clientId, &readFD);
  FD_SET(arg->clientId, &writeFD);
  pthread_mutex_unlock(&mlock);

  pthread_exit(NULL);
}

static int announce_all(struct args *arg, char *alert)
{
  //Announce all clients.
  int type = 2; //Type 2 is for alerts.
  for(int fd = 4;fd<=fdNumber;fd++)
  {
    if(!closed[fd] && FD_ISSET(fd,&writeFD) && fd != arg->clientId)
    {
      if(!write(fd,&type,sizeof(int)))
      {
        printf("[Thread %d]Failed to send alert type to client %d",arg->threadId,fd);
        return 0;
      }
      if(!write_to_client(arg->threadId, fd, alert))
      {
        printf("[Thread %d]Failed to send alert to client %d",arg->threadId,fd);
        return 0;
      }
    }
  }
  return 1;
}

static void read_ready(struct args *arg)
{
  pthread_detach(pthread_self());

  printf("[Thread %d]Ready to read input from Client %d \n",arg->threadId,arg->clientId);
  fflush(stdout);
  int bytes;
  
  //Read the type of the input.
  int type;
  if((bytes=read(arg->clientId,&type,sizeof(int))) <= 0){
    closed[arg->clientId] = 1;
    if(bytes==0){printf("Client %d disconnected.\n",arg->clientId);}
    else printf("[Thread %d]Error at reading the type of input.\n",arg->threadId);
    pthread_exit(NULL);
  }
  fflush(stdout);
  if(type==1)//Speed input
  {

    float speed;
    if((bytes = read(arg->clientId,&speed,sizeof(int))) <= 0)
    {
      closed[arg->clientId] = 1;
      if(bytes==0){printf("Client %d disconnected.\n",arg->clientId);}
      else printf("[Thread %d]Error at reading the speed from client.\n",arg->threadId);
      pthread_exit(NULL);
    }
    clients[arg->clientId].actualSpeed = speed;

    write(arg->clientId, &type,sizeof(int));
    if(speed > 40)
      write_to_client(arg->threadId, arg->clientId, "The speed limit is 40, slow down.");
    else {
      write_to_client(arg->thread,arg->clientId,"The speed limit is 40, your speed is less.");
    }

    printf("[Thread %d]Speed of Client %d is %0.2f\n",arg->threadId,arg->clientId,speed);
    fflush(stdout);
  }
  else{
    if(type==2)//Alert input
    {
      char *alert;
      if(!(alert=(read_from_client(arg->threadId, arg->clientId))))
      {
        printf("[Thread %d]Error at reading the alert from client. \n",arg->threadId);
        pthread_exit(NULL);
      }
      printf("[Thread %d]Client %d : %s\n",arg->threadId,arg->clientId,alert);
      fflush(stdout);

      if(!announce_all(arg,alert)){
        printf("[Thread %d] Failed to send message to all clients.\n",arg->threadId);
        pthread_exit(NULL);
      }

    }
  }
  FD_SET(arg->clientId, &activeFD);
  pthread_exit(NULL);
}

int main() {
  struct sockaddr_in serverStruct = initialize_server();
  struct sockaddr_in clientStruct;

  int socketD;
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
  int first = 1;
  // Wait for the clients to connect and then serve them.
  while (keepRunning) {
    // See if the server was closed
    signal(SIGINT, stopHandler);
    if (!keepRunning)
      break;

    int clientId;
    FD_ZERO(&readFD);
    FD_ZERO(&writeFD);
    bcopy((char *)&activeFD, (char *)&readFD, sizeof(activeFD));
    bcopy((char *)&activeFD, (char *)&writeFD, sizeof(activeFD));

    int nr;

    if (select(fdNumber + 1, &readFD, &writeFD, NULL, &outTime) < 0)
      printError("[Server] Error at select()\n");
    

    if (FD_ISSET(socketD, &readFD)) {
      bzero((struct args *)&arg[index], sizeof(struct args));
      arg[index % 100].socketD = socketD;
      arg[index % 100].threadId = index % 100;
      pthread_create(&arg[index % 100].thread, NULL, (void *)&add_new_client,
                     &arg[index % 100]);
      index++;
    }

    for (fd = 4; fd <= fdNumber; fd++) /* parcurgem multimea de descriptori */
    {
      if (fd != socketD && (FD_ISSET(fd,&readFD))) {
          arg[index].threadId = index;
          arg[index].clientId = fd;
          arg[index].active = 1;
          index++;
          pthread_create(&arg[(index - 1) % 100].thread, NULL, (void *)&read_ready,
                         &arg[(index - 1) % 100]);
          FD_CLR(fd, &activeFD);
        }
    }
  }
  sqlite3_close(db);
  for (fd = 4; fd <= fdNumber; fd++) {
    if (closed[fd] == 0) {
      shutdown(fd, SHUT_RD);
      close(fd);
    }
  }
  close(socketD);
  return 0;
}
