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

const int MAX_CLIENTS = 32;
const int MAX_NUM_MESSAGES = 256;
const int MAX_MESSAGE_LENGTH = 1024;

struct message_queue_t {
  // The array of all messages.  This list is null terminated.
  char **messages;
  int *senders;

  // Where the next push will be.
  size_t pushOffset;
  // Where the next pop will be.
  size_t popOffset;

  size_t maxMessageSize;
  size_t maxNumMessages;

  // Controls simultaneous access to messages
  pthread_mutex_t lock;
};

// The file descriptors for client sockets.  0 indicates an empty slot.
// This is a **SHARED RESOURCE*.  Its access is controlled by
// clientSocketMutex.
int *clientSockets;
pthread_mutex_t clientSocketMutex = PTHREAD_MUTEX_INITIALIZER;

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
  EXIT_ERROR_THREAD,
  EXIT_ERROR_LOCK,
};

// The socket to the remote server/client.
static int remoteSocket;

static struct message_queue_t g_messageQueue;

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

  clientSockets is a **SHARED RESOURCE**.
  clientSocketMutex is used to control access to it.
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
  Returns the next available socket position.

  begin and end are the beginning and end positions of the socket array,
  respectively.

  Returns a pointer if one is found, and NULL otherwise.

  Assumes that there is **no simultaneous access** to the socket array.
  Be sure to lock it beforehand!
*/
int *getNextUnusedSocket(int *begin, int *end);

/*
  Handles a single client connection.

  *args should be a int *fd

  fd is a file descriptor used to communicate with a chat client.

  It will messages that the client connection sends to us into the
  circular queue g_messages.

*/
void * handleConnection(void *args);

/*
  Sends messages received to clients.

  *args should be a int *fd.

  fd is an array of file descriptors, of length MAX_CLIENTS.
*/
void * propagateMessages(void *args);

/*
  Starts a thread that handles message propagation.
*/
void startMessagePropagationThread(int *clientSockets);

/*
  Initializes a message queue.

  bufferSize controls how many messages will fit into the queue at any
  given time.

  Memory is allocated to hold the messages.
*/
void message_queue_init(struct message_queue_t *messageQueue, size_t bufferSize, size_t messageLength);

/*
  Cleans up a message queue.

  Frees up the internal memory buffer.

  You should make sure that no one is using it anymore before cleaning
  up.
*/
void message_queue_cleanup(struct message_queue_t *messageQueue);

/*
  Puts a message into the message queue.

  message is the message to store.
  sender is a unique ID to identify sender is associated with the message.

  May block if the queue is currently full.
*/
void message_queue_put(struct message_queue_t *messageQueue, char *message, int sender);

/*
  Gets a message and its sender from the message queue.

  May block if the queue is currently empty.
*/
void message_queue_get(struct message_queue_t *messageQueue, char *message, int *sender);

/*
  Terminates a string at the first trailing whitespace character.
*/
void nullifyTrailingWhitespace(char *string);

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


  clientSockets = calloc(MAX_CLIENTS, sizeof(int));
  if (clientSockets == NULL) {
    perror(PROG_NAME);
    exit(EXIT_ERROR_MEMORY);
  }

  parseArguments(argc, argv, &PROG_NAME, &socketAddress, &DEBUG);

  message_queue_init(&g_messageQueue, MAX_NUM_MESSAGES, MAX_MESSAGE_LENGTH);

  startMessagePropagationThread(clientSockets);

  listenForClients(socketAddress, clientSockets, MAX_CLIENTS);

  message_queue_cleanup(&g_messageQueue);
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
  int *socket = (int *)(args);
  if (DEBUG) {
    fprintf(stdout, "Listening on FD: %d\n", *socket);
  }
  // This is where we will store the remote client's sent message.
  char messageBuf[256];
  // This should be the same size as messageBuf's size.
  const size_t messageBufsize = 256;

  int chars;
  while ((chars = read(*socket, messageBuf, messageBufsize)) > 0) {
    if (chars < 0) {
      perror(PROG_NAME);
      pthread_exit(NULL);
    }
    messageBuf[chars] = '\0';
    if (DEBUG) {
      fprintf(stderr, "Socket #%d said: '%s'\n", *socket, messageBuf);
    }
    message_queue_put(&g_messageQueue, messageBuf, *socket);
  }

  // ****************************************************************
  // CRITICAL REGION: MODIFYING CLIENT SOCKETS

  // Mark this socket as unusable
  pthread_mutex_lock(&clientSocketMutex);
  if (DEBUG) {
    fprintf(stderr, "Closing socket. FD: %d\n", *socket);
  }
  // Shouldn't error out, but just in case...
  // We shouldn't terminate in here because we STILL HAVE THE MUTEX!!
  if (close(*socket) < 0) {
    perror(PROG_NAME);
  }
  *socket = 0;
  pthread_mutex_unlock(&clientSocketMutex);

  // CRITICAL REGION: MODIFYING CLIENT SOCKETS
  // ****************************************************************
  pthread_exit(NULL);
}

void * propagateMessages(void *args) {
  int *sockets = (int *)args;
  int originalSender;

  char *message = calloc(MAX_MESSAGE_LENGTH, sizeof(char));
  if (message == NULL) {
    perror(PROG_NAME);
    exit(EXIT_ERROR_MEMORY);
  }

  while (true) {
    message_queue_get(&g_messageQueue, message, &originalSender);
    nullifyTrailingWhitespace(message);
    // Print out locally so that server can see what is going on.
    // Maybe can be used to ban foul-mouthed people? :)
    fprintf(stdout, "%s\n", message);

    // ****************************************************************
    // CRITICAL REGION: READING CLIENT SOCKETS

    pthread_mutex_lock(&clientSocketMutex);
    for (size_t i = 0; i < MAX_CLIENTS; ++i) {
      // Don't send a message back to the same client we received it
      // from
      if (sockets[i] != 0 && sockets[i] != originalSender) {
        if (write(sockets[i], message, strlen(message)) < 0) {
          perror(PROG_NAME);
        }
      }
    }
    pthread_mutex_unlock(&clientSocketMutex);

    // CRITICAL REGION: READING CLIENT SOCKETS
    // ****************************************************************
  }
  free(message);
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
    int acceptedSocket = accept(serversocket, &remoteAddress, &remoteAddrLen);
    if (acceptedSocket < 0) {
      perror(PROG_NAME);
      exit(EXIT_ERROR_SOCKET);
    }
    if (DEBUG) {
      fprintf(stderr, "Client connected to socket.\n");
    }

    // ****************************************************************
    // CRITICAL REGION: MODIFYING CLIENT SOCKETS

    pthread_mutex_lock(&clientSocketMutex);
    if ((nextSocket = getNextUnusedSocket(clientSockets, afterLastSocket)) == NULL) {
      // We couldn't find an open slot... reject this client.
      if (DEBUG) {
        fprintf(stderr, "No more free slots.\n");
      }
      close(acceptedSocket);
    } else {
      // An open socket was found!
      if ((pthreadErrno = pthread_create(&threadId, NULL, handleConnection, (void *)(nextSocket))) != 0) {
        fputs("Could not create new thread to handle request.", stderr);
      } else {
        // Everything is successful, let's add it to the active clients
        // list and wait for the next client.
        *nextSocket = acceptedSocket;
      }
    }
    pthread_mutex_unlock(&clientSocketMutex);

    // END CRITICAL REGION: MODIFYING CLIENT SOCKETS
    // ****************************************************************

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

int *getNextUnusedSocket(int *begin, int *end) {
  for (; begin != end; ++begin) {
    if (*begin == 0) {
      return begin;
    }
  }
  return NULL;
}


void startMessagePropagationThread(int *clientSockets) {
  pthread_t threadId;
  int pthreadErrno;

  if (DEBUG) {
    fprintf(stderr, "Starting message propagation thread.\n");
  }
  if ((pthreadErrno = pthread_create(&threadId, NULL, propagateMessages, (void *)(clientSockets))) != 0) {
    perror("PROG_NAME");
    exit(EXIT_ERROR_THREAD);
  }
}


void message_queue_init(struct message_queue_t *messageQueue, size_t bufferSize, size_t messageSize) {
  if (pthread_mutex_init(&messageQueue->lock, NULL) != 0) {
    perror(PROG_NAME);
    exit(EXIT_ERROR_LOCK);
  }

  messageQueue->messages = calloc(bufferSize, sizeof(char *));
  if (messageQueue->messages == NULL) {
    perror(PROG_NAME);
    exit(EXIT_ERROR_MEMORY);
  }

  messageQueue->senders = calloc(bufferSize, sizeof(int));
  if (messageQueue->senders == NULL) {
    perror(PROG_NAME);
    exit(EXIT_ERROR_MEMORY);
  }

  for (size_t i = 0; i < bufferSize - 1; ++i) {
    char *message = calloc(messageSize, sizeof(char));
    if (message == NULL) {
      perror(PROG_NAME);
      exit(EXIT_ERROR_MEMORY);
    }
    messageQueue->messages[i] = message;
  }
  messageQueue->pushOffset = 0;
  messageQueue->popOffset = 0;

  messageQueue->maxMessageSize = messageSize;
  messageQueue->maxNumMessages = bufferSize;

}

void message_queue_cleanup(struct message_queue_t *messageQueue) {
  if (pthread_mutex_destroy(&messageQueue->lock) != 0) {
    perror(PROG_NAME);
    exit(EXIT_ERROR_LOCK);
  }
  // Free up all the individual messages first...
  for (; *messageQueue->messages != NULL; ++messageQueue->messages) {
    free(*messageQueue->messages);
  }
  free(messageQueue->messages);
  free(messageQueue->senders);
}

void message_queue_put(struct message_queue_t *messageQueue, char *message, int sender) {
  pthread_mutex_lock(&messageQueue->lock);

  // Is there space to write to?
  while (messageQueue->pushOffset + 1 == messageQueue->popOffset) {
  // while (messageQueue->nextMessageSlot + 1 == messageQueue->nextMessageToRead) {
    pthread_mutex_unlock(&messageQueue->lock);
    pthread_yield();

    // Try again!
    pthread_mutex_lock(&messageQueue->lock);
  }
  if (!strncpy(messageQueue->messages[messageQueue->pushOffset], message,
    messageQueue->maxMessageSize)) {
    perror(PROG_NAME);
  }
  messageQueue->senders[messageQueue->pushOffset] = sender;
  // Go back to the front if we hit the end
  messageQueue->pushOffset =
    (messageQueue->pushOffset + 1) % messageQueue->maxNumMessages;
  pthread_mutex_unlock(&messageQueue->lock);
}

void message_queue_get(struct message_queue_t *messageQueue, char *message, int *sender) {
  pthread_mutex_lock(&messageQueue->lock);

  // Is there anything to get?
  while (messageQueue->popOffset == messageQueue->pushOffset) {
    pthread_mutex_unlock(&messageQueue->lock);
    pthread_yield();

    // Try again!
    pthread_mutex_lock(&messageQueue->lock);
  }

  if (!strncpy(message, messageQueue->messages[messageQueue->popOffset],
    messageQueue->maxMessageSize)) {
    perror(PROG_NAME);
  }

  *sender = messageQueue->senders[messageQueue->popOffset];

  // Advance to the next message
  // If we're at the last index, go back to the front of the queue.
  messageQueue->popOffset = (messageQueue->popOffset + 1) % messageQueue->maxNumMessages;

  pthread_mutex_unlock(&messageQueue->lock);
}

void nullifyTrailingWhitespace(char *string) {
  char *lastValidChar = string;
  for (; *string != '\0'; ++string)
  {
    if (*string != ' ' && *string != '\t' && *string != '\n')
    {
      lastValidChar = string;
    }
  }

  for (++lastValidChar; *lastValidChar != '\0'; ++lastValidChar)
  {
    *lastValidChar = '\0';
  }
}