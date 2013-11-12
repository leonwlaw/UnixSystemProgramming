/*
Programmer: Leonard Law
Purpose:
  Class project for CS 239.

  This is a simple 1:1 chat server.  Both server and client execute
  this program.  The server will default to client mode if the mode is
  not specified.

  Usage:
    chat [--server] [--debug] [interface] port

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
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
  servermode indicates that the client should operate in server mode.
  debug corresponds to whether the user is requesting debug output.

  If there is an unexpected argument in argv, this will cause the
  program to *TERMINATE*.
*/
void parseArguments(
  int argc, char **argv,
  char **progName,
  struct sockaddr_in *socketAddress, bool *servermode, bool *debug);


/* --------------------------------------------------------------------
Main
-------------------------------------------------------------------- */
int main(int argc, char **argv) {
  bool servermode = false;

  // This is where the program will connect to/bind to.
  struct sockaddr_in socketAddress;
  socketAddress.sin_family = AF_INET;

  parseArguments(argc, argv, &PROG_NAME, &socketAddress, &servermode, &DEBUG);

  if (servermode) {
    if (DEBUG) {
      fputs("Running in server mode.\n", stdout);
    }
    // Used only if we're in server mode. This is where we'll listen for
    // incoming connections.
    int serversocket;

    serversocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serversocket < 0) {
      perror(PROG_NAME);
      exit(EXIT_ERROR_SOCKET);
    }

    socklen_t addrlen = sizeof(socketAddress);

    if (DEBUG) {
      fputs("Binding to socket.\n", stdout);
    }
    if (bind(serversocket, (struct sockaddr *)(&socketAddress), addrlen) != 0) {
      perror(PROG_NAME);
      exit(EXIT_ERROR_SOCKET);
    }

    fputs("Waiting for connection from a host...\n", stdout);

    if (DEBUG) {
      fputs("Connection complete.\n", stdout);
    }
    // Got a connection, exit.
    close(serversocket);
  } else {
    if (DEBUG) {
      fputs("Running in client mode.\n", stdout);
    }
  }


  return 0;
}

/* --------------------------------------------------------------------
Function definitions
-------------------------------------------------------------------- */
void parseArguments(
  int argc, char **argv,
  char **progName,
  struct sockaddr_in *socketAddress, bool *servermode, bool *debug) {

  *progName = *(argv++);

  // Skip the first item, since that points to the executable.
  for (--argc; argc > 0; --argc, ++argv) {
    if (strcmp(*argv, "--server") == 0) {
      *servermode = true;
    } else if (strcmp(*argv, "--debug") == 0) {
      *debug = true;
    } else {
      // This is not a valid option... maybe its an expected argument.
      break;
    }
  }

  // Did the user enter an IP address?
  if (argc == 2) {
    // User entered a pair of IP port values.
    struct in_addr ipv4Address;
    switch (inet_pton(AF_INET, *argv, &ipv4Address)) {
      case 1: // Success
        if (DEBUG) {
          fprintf(stdout, "Accepted interface: %s\n", *argv);
        }
        socketAddress->sin_addr = ipv4Address;
        break;
      case 0: // Not a valid IPv4 address
        fprintf(stderr, "%s: Not a valid address '%s'\n", *progName, *argv);
        exit(EXIT_ERROR_ARGUMENT);
        break;
      case -1: // Socket-related error
        perror(PROG_NAME);
        exit(EXIT_ERROR_SOCKET);
    }

    // Consume the argument.
    --argc;
    ++argv;
  } else {
    // User only entered a port value.
    if (DEBUG) {
      fputs("Did not specify an interface. Listening on all interfaces.\n", stdout);
    }
    socketAddress->sin_addr.s_addr = INADDR_ANY;
  }

  if (argc == 1) {
    // Parse the port value.
    char *afterPort = *argv;
    int port = strtol(*argv, &afterPort, 10);

    // Make sure that we actually consumed a port number, and not a part
    // of the IP address.
    if (*afterPort != '\0') {
      fprintf(stderr, "Invalid port number: '%s'\n", *argv);
      exit(EXIT_ERROR_ARGUMENT);
    }
    if (DEBUG) {
      fprintf(stdout, "Specified port: %d\n", port);
    }
    socketAddress->sin_port = htons(port);

    // Consume the argument.
    --argc;
    ++argv;
  } else {
    fprintf(stderr, "%s: Expected a port number.\n", *progName);
    exit(EXIT_ERROR_ARGUMENT);
  }

  if (*argv != NULL) {
    fprintf(stderr, "%s: Unexpected argument '%s'", *progName, *argv);
    exit(EXIT_ERROR_ARGUMENT);
  }

}
