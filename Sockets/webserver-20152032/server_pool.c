#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>

#include <string.h>
#include "threadpool/thpool.h"

#define PORT 5000

char *ROOT;
void respond(void *arg);

void sendall(int sock, char *msg) {
  int length = strlen(msg);
  int bytes;
  while (length > 0) {
    /* printf("send bytes : %d\n", bytes); */
    bytes = send(sock, msg, length, 0);
    length = length - bytes;
  }
}

int main(int argc, char *argv[]) {
  int newsockfd[50];
  int sockfd, portno = PORT;
  socklen_t clilen;
  struct sockaddr_in serv_addr, cli_addr;

  clilen = sizeof(cli_addr);
  ROOT = getenv("PWD");

  /* First call to socket() function */
  sockfd = socket(AF_INET, SOCK_STREAM, 0);

  if (sockfd < 0) {
    perror("ERROR opening socket");
    exit(1);
  }

  // port reusable
  int tr = 1;
  if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &tr, sizeof(int)) == -1) {
    perror("setsockopt");
    exit(1);
  }

  /* Initialize socket structure */
  bzero((char *)&serv_addr, sizeof(serv_addr));

  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  serv_addr.sin_port = htons(portno);

  /* TODO : Now bind the host address using bind() call.*/
  if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1) {
    perror("bind error");
    exit(1);
  }

  /* TODO : listen on socket you created */

  if (listen(sockfd, 20) == -1) {
    perror("listen error");
    exit(1);
  }

  printf("Server is running on port %d\n", portno);
  threadpool thpool = thpool_init(8);
  while (1) {
    int socket = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
    if (socket == -1) {
      perror("accept error");
      thpool_destroy(thpool);
      exit(1);
    }

    int *arg = (int *)&socket;
    thpool_add_work(thpool, (void *)respond, (void *)arg);
  }

  thpool_destroy(thpool);
  return 0;
}

void getpath(char *buf, char *path) {
  printf("%s\n", buf);
  // check if GET request
  if (strncmp(buf, "GET ", 4) != 0) {
    return;
  }
  buf += 4;
  int counter = 0;
  while (buf[counter] != ' ') {
    counter++;
  }
  memcpy(path, buf, counter);
  path[counter] = '\0';
}

void readpath(char *path, int sock, int bytes) {
  char content[9000];
  FILE *file;
  path++;
  file = fopen(path, "r");

  if (file == NULL) {
    printf("File not found\n");
    return;
  }

  int len = 0;
  char ch;
  char message[1000];

  if (strstr(path, ".css") != NULL) {
    sprintf(message, "HTTP/1.0 200 OK\r\nContent-Type: text/css\r\n\r\n");
  } else {
    sprintf(message, "HTTP/1.0 200 OK\r\nContent-Type: text/html\r\n\r\n");
  }

  int length = strlen(message);
  while (length > 0) {
    bytes = send(sock, message, length, 0);
    length = length - bytes;
  }

  while ((ch = fgetc(file)) != EOF) {
    content[len++] = ch;
    if (len == 9000) {
      content[len] = '\0';
      int length = strlen(content);

      while (length > 0) {
        bytes = send(sock, content, length, 0);
        length = length - bytes;
      }
      len = 0;
    }
  }

  content[len] = '\0';
  length = strlen(content);
  while (length > 0) {
    bytes = send(sock, content, length, 0);
    length = length - bytes;
  }
  fclose(file);
}

void readimage(char *path, int sock, int bytes) {
  char content[9000];
  FILE *file;
  path++;
  file = fopen(path, "rb");

  if (file == NULL) {
    printf("Sad story BOB\n");
    return;
  }

  int len = 0;
  char message[1000];
  sprintf(message, "HTTP/1.0 200 OK\r\nContent-Type: image/jpeg\r\n\r\n");

  int length = strlen(message);
  while (length > 0) {
    bytes = send(sock, message, length, 0);
    length = length - bytes;
  }

  while ((len = fread(content, 1, 1000, file)) > 0) {
    bytes = send(sock, content, len, 0);
  }

  fclose(file);
}

void respond(void *arg) {
  int sock = *(int *)arg;
  int offset, bytes;
  char buffer[9000];
  char path[9000];
  bzero(path, 9000);
  bzero(buffer, 9000);

  offset = 0;
  bytes = 0;
  do {
    // bytes < 0 : unexpected error
    // bytes == 0 : client closed connection
    bytes = recv(sock, buffer + offset, 1500, 0);
    offset += bytes;
    // this is end of http request
    if (strncmp(buffer + offset - 4, "\r\n\r\n", 4) == 0) break;
  } while (bytes > 0);

  if (bytes < 0) {
    printf("recv() error\n");
    return;
  } else if (bytes == 0) {
    printf("Client disconnected unexpectedly\n");
    return;
  }

  buffer[offset] = 0;
  // GET path
  getpath(buffer, path);

  if (strstr(path, ".jpg") != NULL) {
    readimage(path, sock, bytes);
  } else {
    readpath(path, sock, bytes);
  }
  printf("close\n");
  shutdown(sock, SHUT_RDWR);
  close(sock);
}
