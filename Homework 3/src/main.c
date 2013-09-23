#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// The maximum # of characters the user is allowed to input.
size_t INPUT_BUFFERSIZE = 0xFFFF;

// The maximum # of characters the prompt string can be.
size_t PROMPT_BUFFERSIZE = 0xFFFF;

// How many tokens we split each string to be.
size_t TOKEN_BUFFERSIZE = 0xFFF;

char *TOKEN_DELIMITERS = " ";

char *DEFAULT_PROMPT = "> ";
char *EXIT_COMMAND = "exit\n";


// Tokenizes the string as much as the buffer allows.
// If the buffer size is insufficient to store all the tokens,
// this function will return 1.
int tokenize(char *string, char **tokens) {
  int tokenNumber = 0;
  char *token;
  for (token = strtok(string, TOKEN_DELIMITERS);
    // Last spot in the token buffer is reserved for NULL
    token != NULL && tokenNumber < TOKEN_BUFFERSIZE - 1;
    token = strtok(NULL, TOKEN_DELIMITERS), ++tokenNumber)
  {
    tokens[tokenNumber] = token;
  }
  tokens[tokenNumber + 1] = NULL;

  // Did we tokenize everything?
  if (token != NULL)
    return 1;
  return 0;
}


void printStrings(char** printStrings) {
  for (; *printStrings != NULL; ++printStrings) {
    puts(*printStrings);
  }
}


int main(int argc, char const *argv[])
{
  char *input = malloc(sizeof(char) * INPUT_BUFFERSIZE);
  char *prompt = malloc(sizeof(char) * PROMPT_BUFFERSIZE);
  char **tokens = malloc(sizeof(char *) * TOKEN_BUFFERSIZE);

  strncpy(prompt, DEFAULT_PROMPT, PROMPT_BUFFERSIZE);

  while (strcmp(input, EXIT_COMMAND) != 0)
  {
    fputs(prompt, stdout);
    fgets(input, INPUT_BUFFERSIZE, stdin);

    if (tokenize(input, tokens) != 0) {
      fputs("Warning: Not all tokens were tokenized successfully.\n", stderr);
    }
  }

  free(input);
  free(prompt);
  return 0;
}
