#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

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

char *ERRMSG_FORK_FAILED = "Fork failed";

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
int tokenize(char *string, char **tokens, int buffersize);

// Returns 1 if failed to redirect stdin.
int doRedirects(char **tokens, char **arguments);
// Performs any I/O piping if needed.
int setupPipes(char **tokens, char **arguments);

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
/* ----------------------------------------------
Main
-----------------------------------------------*/
int main(int argc, char const *argv[])
{
	char *input = malloc(sizeof(char) * INPUT_BUFFERSIZE);
	char **tokens = malloc(sizeof(char *) * TOKEN_BUFFERSIZE);

	if (input == NULL || tokens == NULL) {
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

	// We should keep the user in the loop until they type the
	// exit command.
	while (1)
	{
		int doFork = 1;

		fputs(prompt, stdout);
		if (fgets(input, INPUT_BUFFERSIZE, stdin) == 0) {
			// No more to read...
			exit(0);
		}

		// Trailing whitespace causes problems with exec...
		nullifyTrailingWhitespace(input);

		if (tokenize(input, tokens, TOKEN_BUFFERSIZE) != 0)
		{
			fputs("Not all tokens were tokenized successfully.\n", stderr);
			doFork = 0;
		}

		if (strcmp(tokens[0], EXIT_COMMAND) == 0) {
			exit(0);
		}

		// Is there even a command to run?
		doFork &= strncmp(tokens[0], "\n", 1) != 0;
		if (doFork) {
			// Run the command and wait for it to complete.
			int p_id = fork();
			if (p_id < 0) {
				perror(ERRMSG_FORK_FAILED);
				exit(FORK_FAILED);

			} else if (p_id == 0) {
				char **arguments = calloc(sizeof(char *), stringArraySize(tokens) + 1);

				if (arguments == NULL) {
					fputs("Could not allocate space for arguments...", stderr);

				} else {
					char **currentProcessTokens = NULL;
					findLastPipedCommand(tokens, &currentProcessTokens);

					// Do we need to pipe?
					while (tokens != currentProcessTokens) {
						int processPipe[2];
						if (pipe(processPipe) != 0) {
							fputs("Error creating pipe.\n", stderr);
							exit(PIPE_FAILED);
						}

						int chain_pid = fork();

						if (chain_pid < 0) {
							perror(ERRMSG_FORK_FAILED);
							exit(FORK_FAILED);

						} else if (chain_pid != 0) {
							// We must redirect stdout to the pipe...
							if (pipeStdout(processPipe) == -1) {
								exit(PIPE_FAILED);
							}

							// Since the parent will be running the last processed
							// command, let's shift the end of the tokens so that
							// we don't re-run the same command.
							*(currentProcessTokens - 1) = NULL;
							findLastPipedCommand(tokens, &currentProcessTokens);

						} else {
							// We are the parent, i.e. we are on the receiving end of
							// the pipe.
							if (pipeStdin(processPipe) == -1) {
								exit(PIPE_FAILED);
							}
							// We are now ready to exec.
							break;
						}
					}

					if (doRedirects(currentProcessTokens, arguments) != 0) {
						fputs("There was an error parsing the arguments.\n", stderr);
					}

					execvp(arguments[0], arguments);
					perror(argv[0]);
					
					free(arguments);
					exit(EXEC_FAILED);
				}

			} else {
				wait(NULL);
			}
		}
	}

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

int tokenize(char *string, char **tokens, int buffersize) {
	// Make sure we leave space for the terminating NULL.
	char ** lastElement = tokens + buffersize - 1;
	char *token;
	for (token = strtok(string, TOKEN_DELIMITERS);
		// Last spot in the token buffer is reserved for NULL
		token != NULL && tokens < lastElement;
		token = strtok(NULL, TOKEN_DELIMITERS), tokens++)
	{
		*tokens = token;
	}

	// Mark the end of the tokens
	*tokens = NULL;

	// Did we tokenize everything?
	if (token != NULL) {
		return 1;
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