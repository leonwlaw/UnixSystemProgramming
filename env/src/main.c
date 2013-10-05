/*
File name:  main.c
Programmer: Leonard Law
Class:      CS 239
Semester:   Fall 2013

Purpose:
	Implementation of env.

*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

extern char **environ;


char *VARIABLE_DELIMITER = "=";

// Printed when the user asks for --help
char *USAGE_STRING[10] = {
	"Usage: env [OPTION]... [NAME=VALUE]... [COMMAND [ARG]...]",
	"Set each NAME to VALUE in the environment and run COMMAND.",
	"",
	"  -i,            start with an empty environment",
	"      --help     display this help and exit",
	"",
	"If no COMMAND, print the resulting environment.",
	"Report env bugs to /dev/null",
	"For complete documentation... none exist. Best of luck!",
	NULL
};


char **getEndOfCStringArray(char **array) {
	char **end;
	// Go to the end of the environment variable
	for (end = array; *end != NULL; ++end)
		; // NULL STATEMENT
	return end;
}


char *getEndOfVariableName(char *string) {
	return strstr(string, VARIABLE_DELIMITER);
}


int isEnvironmentVariable(char *string) {
	// If the '=' is found, then it is a variable.
	return getEndOfVariableName(string) != NULL;
}


size_t calculateNumNewEnvironmentVariables(char **oldEnviron, char **newVariables) {
	size_t numEnvironmentVariables = 0;
	for (; *newVariables != NULL && isEnvironmentVariable(*newVariables); ++newVariables) {
		char *delimiter = getEndOfVariableName(*newVariables);
			if (delimiter != NULL) {
				// Is this something that is already in the environment variable?

				// We want to include the '=' to avoid partial variable name
				// variableNamees. 
				// i.e. a new environment variable 'variable' variableNameing an old
				// variable 'variablename'
				++delimiter;

				int checkStringLength = delimiter - *newVariables;
				int isNewVariable = 0;
				for (char** environmentString = oldEnviron;
					*environmentString != NULL && !isNewVariable;
					++environmentString) {

					isNewVariable = strncmp(*environmentString, *newVariables, checkStringLength) != 0;
				}

				if (isNewVariable != 0) {
					++numEnvironmentVariables;
				}

			} else {
				// This is a program that we want to run
				break;
			}
	}
	return numEnvironmentVariables;
}


char **findFirstNonVariable(char **strings) {
	for (; *strings != NULL; ++strings) {
		if (isEnvironmentVariable(*strings) == 0) {
			break;
		}
	}
	return strings;
}


int overwriteEnvironment(char **target, char **source) {
	for (; *source != NULL && isEnvironmentVariable(*source); ++source) {
		// Find the location to put this new source variable.
		// Prefer an existing location (where a duplicate is), otherwise
		// insert at the end.

		// Variable name and '='
		// i.e. "myvariable=Hello" would return the length of "myvariable="
		size_t checkStringLength = getEndOfVariableName(*source) + 1 - *source;
		char **iterator;
		for (iterator = target; *iterator != NULL; ++iterator) {
			// This is an existing variable, so we want to point this
			// position to the new variable's value.
			if (strncmp(*iterator, *source, checkStringLength) == 0) {
				break;
			}
		}
		*iterator = *source;
	}
	return 0;
}


void printStrings(char** printStrings) {
	for (; *printStrings != NULL; ++printStrings) {
		puts(*printStrings);
	}
}


int env(int ignoreExistingEnvironment, char** args) {
	int numOldEnvironmentVariables = getEndOfCStringArray(environ) - environ;
	int numNewEnvironmentVariables = ignoreExistingEnvironment? 0 : numOldEnvironmentVariables;

	numNewEnvironmentVariables += calculateNumNewEnvironmentVariables(environ, args);

	char** newEnvironment = environ;
	int allocateNewEnvironment = numNewEnvironmentVariables != numOldEnvironmentVariables;

	if (allocateNewEnvironment) {
		// Allocate 1 additional slot for NULL to mark the end of the array
		newEnvironment = calloc(numNewEnvironmentVariables + 1, sizeof(char*));
	}

	// If we allocated a new environment, we need to copy over the old
	// environment to the new one.
	if (!ignoreExistingEnvironment)
		overwriteEnvironment(newEnvironment, environ);
	overwriteEnvironment(newEnvironment, args);
	
	environ = newEnvironment;

	// User specified a non-variable argument.
	// This means they intend to run a program with the modified
	// environment.
	newEnvironment[numNewEnvironmentVariables] = NULL;

	char **nonVariableArgument = findFirstNonVariable(args);
	if (*nonVariableArgument != NULL) {
		execvp(nonVariableArgument[0], nonVariableArgument);
		perror("env");
		exit(1);
	}

	// Default behavior, simply print out everything in the
	// environment variable
	printStrings(newEnvironment);

	if (allocateNewEnvironment)
		free(newEnvironment);

	return 0;
}


int main(int argc, char **argv) {
	int ignoreExistingEnvironment = 0;

	// Is the user attempting to pass a command line argument?
	if (argc > 1 && (strncmp(argv[1], "-", 1) == 0)) {
		// Check any recognized flags...
		if (strcmp(argv[1], "-i") == 0) {
			ignoreExistingEnvironment = (strcmp(argv[1], "-i") == 0);

		} else if (strcmp(argv[1], "--help") == 0) {
			printStrings(USAGE_STRING);
			exit(0);

		} else {
			// Not recognized.  Print error string and die.
			fprintf(stderr, "env: unrecognized option '%s'\n", argv[1]);
			fprintf(stderr, "Try 'env --help' for more information.\n");
			exit(1);
		}
	}

	if (ignoreExistingEnvironment) {
		env(ignoreExistingEnvironment, argv + 2);
	} else {
		env(ignoreExistingEnvironment, argv + 1);
	}
}
