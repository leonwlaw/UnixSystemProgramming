/*
Programmer: Leonard Law
Purpose:
  Class project for CS 239.

  This is a chat server that enables multiple clients to communicate
  with each other.

  Usage:
    server [--debug] [interface] port

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <stdbool.h>
#include <pthread.h>

char *PROG_NAME;
bool DEBUG = false;

const int MAX_CLIENTS = 2;
const int MESSAGE_BUFSIZE = 4096;

// The file descriptors for client sockets.  0 indicates an empty slot.
int *clientSockets;

char *SEPARATOR = ": ";
size_t SEPARATOR_LENGTH = 2;

// Sentinel value to indicate an invalid or otherwise NULL file
// descriptor.
const int FD_NULL = -1;

// The set of valid exit values.
enum EXIT_T {
  EXIT_NORMAL = 0,
  EXIT_ERROR_ARGUMENT,
  EXIT_ERROR_SOCKET,
  EXIT_ERROR_MEMORY,
  EXIT_ERROR_IO,
};

// The socket to the remote server/client.
static int remoteSocket;

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
  int argc, char **argv, char **progName,
  struct sockaddr_in *socketAddress, bool *debug);


/*
  Waits for a client to connect to us.

  socketAddress is the address where we should listen on.

  clientSockets are file descriptors used to communicate to/from the
  remote client.  This is treated as a circular buffer.

  numClients are the maximum number of clients.
*/
void listenForClients(struct sockaddr_in socketAddress, int *clientSockets, int numClients);

/*
  Close any remote connections before exiting.
*/
void closeRemoteConnection();

/*
  Prints out the usage string for this program.
*/
void displayUsageString();


/*
  Sets current to be the next available socket position.

  begin and end are the beginning and end positions of the socket array,
  respectively.

  Returns 0 if successful.  Returns -1 if none exist.
*/
int getNextUnusedSocket(int **current, int *begin, int *end);

/*
  Handles a single client connection.

  *args should be a int *fd

  fd is a file descriptor used to communicate with a chat client.

  It will messages that the client connection sends to us into the
  circular queue g_messages.

*/
void * handleConnection(void *args);

/* --------------------------------------------------------------------
Main
-------------------------------------------------------------------- */
int main(int argc, char **argv) {
  remoteSocket = FD_NULL;
  // We should close the remote connection so that the remote end does
  // not end up with a socket stuck in TIME_WAIT.
  atexit(closeRemoteConnection);

  // This is where the program will connect to/bind to.
  struct sockaddr_in socketAddress;
  socketAddress.sin_family = AF_INET;

  clientSockets = calloc(sizeof(int), MAX_CLIENTS);
  if (clientSockets == NULL) {
    perror(PROG_NAME);
    exit(EXIT_ERROR_MEMORY);
  }

  parseArguments(argc, argv, &PROG_NAME, &socketAddress, &DEBUG);

  listenForClients(socketAddress, clientSockets, MAX_CLIENTS);
  free(clientSockets);
  return 0;
}

/* --------------------------------------------------------------------
Function definitions
-------------------------------------------------------------------- */
void parseArguments(
  int argc, char **argv, char **progName,
  struct sockaddr_in *socketAddress, bool *debug) {

  *progName = *(argv++);

  // Skip the first item, since that points to the executable.
  for (--argc; argc > 0; --argc, ++argv) {
    if (strcmp(*argv, "--debug") == 0) {
      *debug = true;
    } else {
      // This is not a valid option... maybe its an expected argument.
      break;
    }
  }

  // Did the user enter an IP address?
  if (argc == 3) {
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
        displayUsageString();
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

  if (*argv == NULL) {
    fputs("Expected a port number\n", stderr);
    displayUsageString();
    exit(EXIT_ERROR_ARGUMENT);
  }

  // Parse the port value.
  char *afterPort = *argv;
  int port = strtol(*argv, &afterPort, 10);

  // Make sure that we actually consumed a port number, and not a part
  // of the IP address.
  if (*afterPort != '\0') {
    fprintf(stderr, "Invalid port number: '%s'\n", *argv);
    displayUsageString();
    exit(EXIT_ERROR_ARGUMENT);
  }
  if (DEBUG) {
    fprintf(stdout, "Specified port: %d\n", port);
  }
  socketAddress->sin_port = htons(port);

  // Consume the argument.
  --argc;
  ++argv;

  if (*argv != NULL) {
    fprintf(stderr, "%s: Unexpected argument '%s'", *progName, *argv);
    displayUsageString();
    exit(EXIT_ERROR_ARGUMENT);
  }

}

void * handleConnection(void *args) {
  // The file descriptor of the remote socket.
  int socket = *(int *)(args);
  fprintf(stdout, "Listening on FD: %d\n", socket);
  // This is where we will store the remote client's sent message.
  char messageBuf[256];
  // This should be the same size as messageBuf's size.
  const size_t messageBufsize = 256;

  int chars;
  while ((chars = read(socket, messageBuf, messageBufsize)) > 0) {
    if (chars < 0) {
      perror(PROG_NAME);
      pthread_exit(NULL);
    }
    messageBuf[chars] = '\0';
    fputs(messageBuf, stdout);
  }
  pthread_exit(NULL);
}


void listenForClients(struct sockaddr_in socketAddress, int *clientSockets, int numClients) {
  // Used only if we're in server mode. This is where we'll listen for
  // incoming connections.
  if (DEBUG) {
    fputs("Running in server mode.\n", stdout);
  }
  int serversocket = socket(AF_INET, SOCK_STREAM, 0);
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

  if (listen(serversocket, MAX_CLIENTS) != 0) {
    perror(PROG_NAME);
    exit(EXIT_ERROR_SOCKET);
  }

  struct sockaddr remoteAddress;
  socklen_t remoteAddrLen = sizeof(remoteAddress);

  int *nextSocket = clientSockets;
  int *afterLastSocket = clientSockets + MAX_CLIENTS;

  pthread_t threadId;
  int pthreadErrno;

  while (true) {
    if (nextSocket < afterLastSocket && *nextSocket == 0) {
      if ((*nextSocket = accept(serversocket, &remoteAddress, &remoteAddrLen)) == -1) {
        perror(PROG_NAME);
        exit(EXIT_ERROR_SOCKET);
      }
      if (DEBUG) {
        fprintf(stderr, "Client connected to socket.\n");
      }
      if ((pthreadErrno = pthread_create(&threadId, NULL, handleConnection, (void *)(nextSocket))) != 0) {
        fputs("Could not create new thread to handle request.", stderr);
      }
    }

    if ((getNextUnusedSocket(&nextSocket, clientSockets, afterLastSocket) != 0) && DEBUG) {
      fprintf(stderr, "No more free slots.\n");
      // We should perhaps wait until a port reopens instead of doing this...
      // Remove this when we're ready.
      fprintf(stderr, "Pausing forever until we actually find an unused socket.\n");
      while (true) {
        sleep(1);
      }
      break;
    }
  }

  // Got a connection, exit.
  close(serversocket);
}

int writeToFile(int file, char *message, size_t chars) {
  ssize_t bytesWritten = 0;
  while ((bytesWritten = write(file, message + bytesWritten, chars - bytesWritten))) {
    if (bytesWritten < 0) {
      return 1;
    }
  }
  return 0;
}

void connectToServer(struct sockaddr_in *socketAddress, int *remotesocket) {
  // Used only if we're in client mode. Connect to the remote server.
  if (DEBUG) {
    fputs("Running in client mode.\n", stdout);
  }

  *remotesocket = socket(AF_INET, SOCK_STREAM, 0);
  if (*remotesocket < 0) {
    perror(PROG_NAME);
    exit(EXIT_ERROR_SOCKET);
  }

  socklen_t addrlen = sizeof(*socketAddress);

  if (DEBUG) {
    fputs("Connecting to server...\n", stdout);
  }

  if (connect(*remotesocket, (struct sockaddr *)(socketAddress), addrlen) != 0) {
    perror(PROG_NAME);
    exit(EXIT_ERROR_SOCKET);
  }

  if (DEBUG) {
    fputs("Server connected.\n", stdout);
  }
}

void closeRemoteConnection() {
  if (remoteSocket != FD_NULL) {
    if (close(remoteSocket) != 0) {
      perror(PROG_NAME);
    }
  }
}

void displayUsageString() {
  fputs("Usage:\n\
    server [--debug] [interface] port\n", stdout);
}

int getNextUnusedSocket(int **current, int *begin, int *end) {
  ++(*current);
  if (*current == end) {
    return -1;
  }
  return 0;
}
