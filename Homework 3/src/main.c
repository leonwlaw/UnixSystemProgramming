#include <stdio.h>
#include <stdlib.h>
#include <string.h>

size_t INPUT_BUFFERSIZE = 4096;
size_t PROMPT_BUFFERSIZE = 4096;

char *DEFAULT_PROMPT = "> ";
char *EXIT_COMMAND = "exit\n";


int main(int argc, char const *argv[])
{
  char *input = malloc(sizeof(char) * INPUT_BUFFERSIZE);
  char *prompt = malloc(sizeof(char) * PROMPT_BUFFERSIZE);

  strncpy(prompt, DEFAULT_PROMPT, PROMPT_BUFFERSIZE);

  while (strcmp(input, EXIT_COMMAND) != 0)
  {
    fputs(prompt, stdout);
    fgets(input, INPUT_BUFFERSIZE, stdin);
    fputs(input, stdout);
  }

  free(input);
  free(prompt);
  return 0;
}
