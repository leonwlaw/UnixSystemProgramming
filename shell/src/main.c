#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wait.h>

// The maximum # of characters the user is allowed to input.
size_t INPUT_BUFFERSIZE = 0xFFFF;

// The maximum # of characters the prompt string can be.
size_t PROMPT_BUFFERSIZE = 0xFFFF;

// How many tokens we split each string to be.
size_t TOKEN_BUFFERSIZE = 0xFFF;

char *TOKEN_DELIMITERS = " ";

char *DEFAULT_PROMPT = "> ";
char *EXIT_COMMAND = "exit";


int stringArraySize(char ** array) {
	size_t size = 0;
	for (; *array != NULL; ++size, ++array)
		; // NULL
	return size;
}


// Tokenizes the string as much as the buffer allows.
// If the buffer size is insufficient to store all the tokens,
// this function will return 1.
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


int parseArgumentsIORedirection(char **tokens, char **arguments) {
	for (; *tokens != NULL; ++tokens) {
		*arguments = *tokens;
		++arguments;
	}
	return 0;
}


int main(int argc, char const *argv[])
{
	char *input = malloc(sizeof(char) * INPUT_BUFFERSIZE);
	char **tokens = malloc(sizeof(char *) * TOKEN_BUFFERSIZE);

	char *prompt = getenv("PS1");
	if (prompt == NULL) {
		prompt = malloc(sizeof(char) * PROMPT_BUFFERSIZE);
		strncpy(prompt, DEFAULT_PROMPT, PROMPT_BUFFERSIZE);
	}

	// We should keep the user in the loop until they type the
	// exit command.
	while (1)
	{
		int doFork = 1;

		fputs(prompt, stdout);
		fgets(input, INPUT_BUFFERSIZE, stdin);

		// Trailing whitespace causes problems with exec...
		nullifyTrailingWhitespace(input);

		if (tokenize(input, tokens, TOKEN_BUFFERSIZE) != 0)
		{
			fputs("Not all tokens were tokenized successfully.\n", stderr);
			doFork = 0;
		}

		char **arguments = calloc(sizeof(char *), stringArraySize(arguments));

		if (parseArgumentsIORedirection(tokens, arguments) != 0) {
			fputs("There was an error parsing the arguments.\n", stderr);
			doFork = 0;
		}

		if (strcmp(arguments[0], EXIT_COMMAND) == 0) {
			exit(0);
		}

		if (doFork) {
			// Run the command and wait for it to complete.
			int p_id = fork();
			if (p_id == 0) {
				execvp(arguments[0], arguments);
				perror(argv[0]);
				exit(1);
			} else {
				wait(NULL);
			}
		}

		free(arguments);
	}

	free(input);
	free(prompt);
	return 0;
}

