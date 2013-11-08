#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wait.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdbool.h>
#include <fcntl.h>
#include <glob.h>

/* ----------------------------------------------
Constants
-----------------------------------------------*/

// The maximum # of characters the user is allowed to input.
size_t INPUT_BUFFERSIZE = 0xFFFF;

// The maximum # of characters the prompt string can be.
size_t PROMPT_BUFFERSIZE = 0xFFFF;

// How many tokens we split each string to be.
size_t TOKEN_BUFFERSIZE = 0xFFF;

char *TOKEN_DELIMITERS = " ";
char *PIPE_DELIMITER = "|";

char *DEFAULT_PROMPT = "> ";
char *EXIT_COMMAND = "exit";

int FILE_INDEX_STDIN = 0;
int FILE_INDEX_STDOUT = 1;

int NOT_ENOUGH_MEMORY = 1;
int EXEC_FAILED = 2;
int PIPE_FAILED = 3;
int FORK_FAILED = 4;
int SIGACTION_ERROR = 5;
int FOREGROUND_SWAP_ERROR = 6;

char *ERRMSG_FORK_FAILED = "Fork failed";

/* ----------------------------------------------
Global Variables
-----------------------------------------------*/
// The currently running process group ID.
// 0 means no process group is being run in this shell.
int active_pgid = 0;

// The next background job ID to use.
int nextjobID = 0;

/* ----------------------------------------------
Function prototypes 
-----------------------------------------------*/

// Sets thisProcTokens to the beginning of the last command in the
// chain of piped commands. This will point immediately after the
// command separator.
void findLastPipedCommand(char **tokens, char ***thisProcTokens);


// Calculates the length of the string array.
int stringArraySize(char **array);

// Tokenizes the string as much as the buffer allows.
// If the buffer size is insufficient to store all the tokens,
// this function will return 1.
int tokenize(char *string, glob_t *globbuf);

// Returns 1 if failed to redirect stdin.
int doRedirects(char **tokens, char **arguments);

// Transparently sets up any piping between processes as needed.
// This will also call fork() as needed to connect the processes' pipes
// Returns 0 on normal operation. Otherwise, the return value corresponds
// to the error constants listed above.
int setupPipesAndFork(char **tokens, char ***processTokens);

// Sets stdin to point to the reading end of the specified pipe.
// *pipe is a pair of int pointers, obtained from pipe().
// Returns -1 upon failure, or 0 if successful.
int pipeStdin(int *pipe);

// Sets stdout to point to the writing end of the specified pipe.
// *pipe is a pair of int pointers, obtained from pipe().
// Returns -1 upon failure, or 0 if successful.
int pipeStdout(int *pipe);

// Finds the last non-whitespace character in the string and replaces
// it with a null.
void nullifyTrailingWhitespace(char *string);

// This function will handle SIGINTs so that they do not terminate
// the shell.  It will instead terminate the current child that we
// are waiting on, if one exists.
void propagateSignalToChildProcesses(int signal);

// Sets up signal propagation so that signals relevant signals are sent
// to the current running child process that we are actively waiting for
void setupSignalPropagationToChild();

// Obtains input from stdin.  It will also print out a prompt, and
// output any necessary characters to ensure that the output to stdout
// isn't butchered. (i.e. due to signals signals, like SIGINT)
void getCommandWithPrompt(char *input, char *prompt);

// Change foreground status to a child that we started.
// childpid is the child's PID. oldActivePgid and oldSigaction will be
// modified to reflect the process's status prior to calling this
// function, which can be passed to restoreForegroundToSelf later to
// undo this function's effects.
void changeForegroundToChild(
	pid_t childpid, int *oldActivePgid, struct sigaction *oldSigaction);

// Restore foreground process status back to ourself.
// oldActivePgid and oldsigaction should be arguments obtained from
// calling changeForegroundToChild.
void restoreForegroundToSelf(
	int oldActivePgid, struct sigaction *oldsigaction);

// Wait for the specified process group to terminate.
void waitForProcessGroup(int pgrpid);

// Waits for any backgrounded children to dezombify them.
void waitForBackgroundedChildren();

/* ----------------------------------------------
Main
-----------------------------------------------*/
int main(int argc, char const *argv[])
{
	char *input = malloc(sizeof(char) * INPUT_BUFFERSIZE);

	if (input == NULL) {
		fputs("Not enough memory.", stderr);
		exit(NOT_ENOUGH_MEMORY);
	}

	char *prompt = getenv("PS1");
	if (prompt == NULL) {
		prompt = malloc(sizeof(char) * PROMPT_BUFFERSIZE);
		if (prompt == NULL) {
			fputs("Not enough memory.", stderr);
			exit(NOT_ENOUGH_MEMORY);
		}
		strncpy(prompt, DEFAULT_PROMPT, PROMPT_BUFFERSIZE);
	}

	// This is where we'll store the parsed argument tokens
	glob_t globbuf;

	setupSignalPropagationToChild();

	// We should keep the user in the loop until they type the
	// exit command.
	while (true)
	{
		getCommandWithPrompt(input, prompt);

		// Trailing whitespace causes problems with exec...
		nullifyTrailingWhitespace(input);

		if (tokenize(input, &globbuf) != 0)
		{
			fputs("Not all tokens were tokenized successfully.\n", stderr);
		}

		int numTokens = globbuf.gl_pathc;
		char **tokens = &globbuf.gl_pathv[0];

		if (strcmp(tokens[0], EXIT_COMMAND) == 0) {
			exit(0);
		}

		char **argStart = tokens;
		char **argEnd = tokens;

		int lastPid;
		int forkDone = false;
		int lastBackgrounded = false;

		// Figure out which commands constitute the next process group to run.
		for (size_t i = 0; i < numTokens; ++i) {
			int doFork = 0;
			if (strcmp(*(tokens + i), "&") == 0) {
				argEnd = tokens + i;
				if (argStart == argEnd) {
					fputs("supershell: syntax error\n", stderr);
					// No reason to go onto the next command...
					// At this point, any processes that we've run are
					// backgrounded, so we don't have to wait for anyone
					// *YET*.
					break;
				} else {
					doFork = 1;
					lastBackgrounded = true;
				}
			}
			else if (i == numTokens - 1) {
				argEnd = tokens + numTokens + 1;
				doFork = 1;
				lastBackgrounded = false;
			}

			// Is there even a command to run?
			doFork &= (*argStart != NULL) && (strcmp(argStart[0], "\n") != 0);
			if (doFork) {
				lastPid = fork();

				if (lastPid < 0) {
					perror(ERRMSG_FORK_FAILED);
					exit(FORK_FAILED);

				} else if (lastPid == 0) {
					int my_pid = getpid();
					if (setpgid(my_pid, my_pid) != 0) {
						perror("Change process group");
						exit(FORK_FAILED);
					}

					char **arguments = calloc(sizeof(char *), argEnd - argStart + 1);

					if (arguments == NULL) {
						fputs("Could not allocate space for arguments...", stderr);

					} else {
						// Copy over the arguments so that we don't butcher the 
						char **from, **to;
						for (from = argStart, to = arguments;
							from != argEnd;
							++from, ++to) {

							*to = *from;
						}
						*to = NULL;

						char **processTokens;
						int setupPipesSuccess = setupPipesAndFork(arguments, &processTokens);
						if (setupPipesSuccess != 0) {
							fputs("Setting up pipes between processes failed.\n", stderr);
							exit(setupPipesSuccess);
						}

						if (doRedirects(processTokens, arguments) != 0) {
							fputs("There was an error parsing the arguments.\n", stderr);
						}

						execvp(arguments[0], arguments);
						perror(argv[0]);

						free(arguments);
						exit(EXEC_FAILED);
					}

				} else {
					forkDone = true;
					if (lastBackgrounded) {
						fprintf(stdout, "[%i] %d\n", nextjobID++, lastPid);
					}
				}
				argStart = argEnd + 1;
			}
		}
		if (forkDone && !lastBackgrounded) {
			waitForProcessGroup(lastPid);
		}
		waitForBackgroundedChildren();
	}

	globfree(&globbuf);
	free(input);
	free(prompt);
	return 0;
}

/* ----------------------------------------------
Function definitions
-----------------------------------------------*/

int doRedirects(char **tokens, char **arguments) {
	char noMoreRedirects = '\0';

	char redirectChar[] = {'<', '>', noMoreRedirects};
	int redirectFileMode[] = {
		O_RDONLY,
		O_WRONLY | O_CREAT | O_TRUNC,
		0
	};

	int redirectFilePermission[] = {
		0,
		// Assume that the output file is not an executable...
		0666,
		0
	};

	for (; *tokens != NULL; ++tokens) {
		int redirected = 0;
		// Try to see if there are any redirect characters that match.
		for (int fd = 0; *(redirectChar + fd) != noMoreRedirects; ++fd) {
			redirected = strchr(*tokens, redirectChar[fd]) != 0;
			if (redirected) {
				// The filename is contained in the next token.
				++tokens;
				char *filename = *tokens;	
				if (close(fd) == -1) {
					perror("I/O redirection");
					return 1;
				}
				if (open(filename, redirectFileMode[fd], redirectFilePermission[fd]) == -1) {
					perror("I/O redirection");
					return 1;
				}
				// There's no point in looking for more, if this one matches.
				break;
			}
		}

		if (!redirected) {
			*arguments = *tokens;
			++arguments;
		}
	}
	*arguments = NULL;
	return 0;
}


void findLastPipedCommand(char **tokens, char ***thisProcTokens) {
	char **commandBegin = tokens;
	for (; *tokens != NULL; ++tokens) {
		if (strcmp(*tokens, PIPE_DELIMITER) == 0) {
			commandBegin = ++tokens;
		}
	}

	*thisProcTokens = commandBegin;
}


int stringArraySize(char **array) {
	size_t size = 0;
	for (; *array != NULL; ++size, ++array)
		; // NULL
	return size;
}

int tokenize(char *string, glob_t *globbuf) {
	// Make sure we leave space for the terminating NULL.
	char *token;
	int firstToken = true;
	int globSuccess;
	for (token = strtok(string, TOKEN_DELIMITERS);
		// Last spot in the token buffer is reserved for NULL
		token != NULL;
		token = strtok(NULL, TOKEN_DELIMITERS))
	{
		int globflags = firstToken ? GLOB_NOCHECK : GLOB_NOCHECK | GLOB_APPEND;
		if ((globSuccess = glob(token, globflags, NULL, globbuf)) != 0) {
			fprintf(stderr, "Glob failed with return value %d\n", globSuccess);
		}
		firstToken &= false;
	}

	return 0;
}


void nullifyTrailingWhitespace(char *string)
{
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

int pipeStdout(int *pipe) {
	if (dup2(pipe[FILE_INDEX_STDOUT], FILE_INDEX_STDOUT) != FILE_INDEX_STDOUT) {
		fprintf(stderr, "Failed to pipe stdout.");
		return -1;
	}
	if (close(pipe[FILE_INDEX_STDIN]) == -1) {
		fprintf(stderr, "Failed to close unused pipe stdin.");
		return -1;
	}
	return 0;
}

int pipeStdin(int *pipe) {
	if (dup2(pipe[FILE_INDEX_STDIN], FILE_INDEX_STDIN) != FILE_INDEX_STDIN) {
		fprintf(stderr, "Failed to pipe stdin.");
		return -1;
	}
	if (close(pipe[FILE_INDEX_STDOUT]) == -1) {
		fprintf(stderr, "Failed to close unused pipe stdout.");
		return -1;
	}
	return 0;
}

int setupPipesAndFork(char **tokens, char ***processTokens) {
	findLastPipedCommand(tokens, processTokens);

	// Do we need to pipe?
	while (tokens != *processTokens) {
		int processPipe[2];
		if (pipe(processPipe) != 0) {
			fputs("Error creating pipe.\n", stderr);
			return PIPE_FAILED;
		}

		int chain_pid = fork();

		if (chain_pid < 0) {
			perror(ERRMSG_FORK_FAILED);
			return FORK_FAILED;

		} else if (chain_pid != 0) {
			// We must redirect stdout to the pipe...
			if (pipeStdout(processPipe) == -1) {
				return PIPE_FAILED;
			}

			// Since the parent will be running the last processed
			// command, let's shift the end of the tokens so that
			// we don't re-run the same command.
			*(*processTokens - 1) = NULL;
			findLastPipedCommand(tokens, processTokens);

		} else {
			// We are the parent, i.e. we are on the receiving end of
			// the pipe.
			if (pipeStdin(processPipe) == -1) {
				return PIPE_FAILED;
			}
			// We are now ready to exec.
			break;
		}
	}
	return 0;
}

void propagateSignalToChildProcesses(int signum) {
	if (active_pgid != 0) {
		// Only propagate a signal if there is an actively running
		// process group that we're waiting on.
		if (kill(-active_pgid, signum) != 0) {
			perror("kill");
		}
	}
}

void setupSignalPropagationToChild() {
	struct sigaction oldSigaction;
	struct sigaction newSigaction;
	sigemptyset(&newSigaction.sa_mask);
	newSigaction.sa_flags = 0;
	newSigaction.sa_handler = propagateSignalToChildProcesses;
	if (sigaction(SIGINT, &newSigaction, &oldSigaction) != 0) {
		perror("sigaction: sigint");
		exit(SIGACTION_ERROR);
	}

	if (sigaction(SIGQUIT, &newSigaction, &oldSigaction) != 0) {
		perror("sigaction: sigquit");
		exit(SIGACTION_ERROR);
	}
}


void getCommandWithPrompt(char *input, char *prompt) {
	fputs(prompt, stdout);
	if (fgets(input, INPUT_BUFFERSIZE, stdin) == NULL) {
		if (feof(stdin) == false) {
			// This means that the user issued a SIGINT, so we forget about the
			// current input and ask for a new prompt...
			strcpy(input, "\n");
		} else {
			// We are at EOF.  Tell the user that we're exiting...
			fputs("exit", stdout);
			strcpy(input, "exit");
		}
		// Put a newline so that the prompt doesn't get mixed up with the
		// current line's input.
		putc('\n', stdout);
	}
}


void changeForegroundToChild(pid_t childpid, int *oldActivePgid, struct sigaction *prebackgroundSigaction) {
	// Indicate that we're running something, so
	// that signals that the user sends to this
	// process are propagated there.
	active_pgid = childpid;

	// Change child process group to be the foreground
	// process

	// We need to block SIGTTOU since it is sent when we
	// are giving away foreground status to a child process
	// by calling tcsetpgrp.
	struct sigaction newsigaction;
	sigemptyset(&newsigaction.sa_mask);
	newsigaction.sa_flags = 0;
	newsigaction.sa_handler = SIG_IGN;

	if (sigaction(SIGTTOU, &newsigaction, prebackgroundSigaction) != 0) {
		perror("Foreground");
		exit(SIGACTION_ERROR);
	}

	*oldActivePgid = tcgetpgrp(0);
	if (*oldActivePgid != -1) {
		if (tcsetpgrp(0, childpid) != 0) {
			perror("Background");
			exit(FOREGROUND_SWAP_ERROR);
		}
	}
}


void restoreForegroundToSelf(int oldActivePgid, struct sigaction *oldsigaction) {
	if (oldActivePgid != -1) {
		// We are done, restore foreground status and stop
		// ignoring SIGTTOU.
		if (tcsetpgrp(0, oldActivePgid) != 0) {
			perror("Foreground");
			exit(FOREGROUND_SWAP_ERROR);
		}
	}

	if (sigaction(SIGTTOU, oldsigaction, NULL) != 0) {
		perror("Foreground");
		exit(SIGACTION_ERROR);
	}

	// Indicate that we're no longer running
	// anything...
	active_pgid = 0;
}

void waitForProcessGroup(int pgrpid) {
	int oldActivePgid;
	int terminatedPid;
	int processStatus;
	struct sigaction prebackgroundSigaction;
	changeForegroundToChild(pgrpid, &oldActivePgid, &prebackgroundSigaction);

	while ((terminatedPid = waitpid(pgrpid, &processStatus, 0)) != pgrpid) {
		// Do nothing...
	}

	restoreForegroundToSelf(oldActivePgid, &prebackgroundSigaction);
	fflush(stdout);
	fflush(stderr);
}


void waitForBackgroundedChildren() {
	int terminatedPid;
	int processStatus;
	while ((terminatedPid = waitpid(-1, &processStatus, WNOHANG)) != 0) {
		// This is fine, as we don't have a good way of keeping track of
		// how many children we're waiting on...
		if (terminatedPid == -1 && errno == ECHILD) {
			errno = 0;
			break;
		}
		fprintf(stdout, "Process %d has terminated.\n", terminatedPid);
	}
}

