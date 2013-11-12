/*
Programmer: Leonard Law
Purpose:
  Class project for CS 239.

  This is a simple 1:1 chat server.  Both server and client execute
  this program.  The server will default to server mode if the mode is
  not specified.

  Usage:
    chat --server [interface] port
    chat --client [interface] port

*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>

char *PROG_NAME;

// The set of valid exit values.
enum EXIT_T {
  EXIT_NORMAL = 0,
  EXIT_ERROR_SOCKET,
};

int main(int argc, char **argv) {
  PROG_NAME = argv[0];

  // Used only if we're in server mode. This is where we'll listen for
  // incoming connections.
  int serversocket;

  serversocket = socket(AF_INET, SOCK_STREAM, 0);
  if (serversocket < 0) {
    perror(PROG_NAME);
    exit(EXIT_ERROR_SOCKET);
  }

  fputs("Waiting for connection from a host...\n", stdout);

  // Got a connection, exit.
  close(serversocket);
  return 0;
}
