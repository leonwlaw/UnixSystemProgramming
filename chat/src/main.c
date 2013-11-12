/*
Programmer: Leonard Law
Purpose:
  Class project for CS 239.

  This is a simple 1:1 chat server.  Both server and client execute
  this program.  The server will default to server mode if the mode is
  not specified.

  Usage:
    chat --server [interface] port [--debug]
    chat --client [interface] port [--debug]

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdbool.h>

char *PROG_NAME;
bool DEBUG = false;

// The set of valid exit values.
enum EXIT_T {
  EXIT_NORMAL = 0,
  EXIT_ERROR_ARGUMENT,
  EXIT_ERROR_SOCKET,
};

/* --------------------------------------------------------------------
Function declarations
-------------------------------------------------------------------- */

/*
  Interprets command line arguments.
  argc and argv are the # of command line arguments, and the array of
  command line arguments respectively.

  progName is the string pointer that should hold the executable's name.
  debug corresponds to whether the user is requesting debug output.

  If there is an unexpected argument in argv, this will cause the
  program to *TERMINATE*.
*/
void parseArguments(
  int argc, char **argv,
  char **progName, bool *debug);


/* --------------------------------------------------------------------
Main
-------------------------------------------------------------------- */
int main(int argc, char **argv) {
  parseArguments(argc, argv, &PROG_NAME, &DEBUG);

  // Used only if we're in server mode. This is where we'll listen for
  // incoming connections.
  int serversocket;

  serversocket = socket(AF_INET, SOCK_STREAM, 0);
  if (serversocket < 0) {
    perror(PROG_NAME);
    exit(EXIT_ERROR_SOCKET);
  }

  fputs("Waiting for connection from a host...\n", stdout);

  if (DEBUG) {
    fputs("Connection complete.\n", stdout);
  }
  // Got a connection, exit.
  close(serversocket);
  return 0;
}

/* --------------------------------------------------------------------
Function definitions
-------------------------------------------------------------------- */
void parseArguments(
  int argc, char **argv,
  char **progName, bool *debug) {

  *progName = *(argv++);

  // Skip the first item, since that points to the executable.
  for (--argc; argc > 0; --argc, ++argv) {
    if (strcmp(*argv, "--debug") == 0) {
      *debug = true;
    } else {
      fprintf(stderr, "%s: Unexpected argument '%s'", *progName, *argv);
      exit(EXIT_ERROR_ARGUMENT);
    }
  }
}
