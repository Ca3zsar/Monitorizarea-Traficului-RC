#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sqlite3.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

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

int main() { return 0; }
