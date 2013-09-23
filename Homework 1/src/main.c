/*
File name:  main.c
Programmer: Leonard Law
Class:      CS 239
Semester:   Fall 2013

Purpose:
    Performs a simulation of the game of life.

    The program expects to find a file named "life.txt" which will
    contain a default state for the program. This state can be
    overwritten by providing command line arguments.

    [rows columns [filename [generations]]]

*/
#include <stdlib.h>
#include <stdio.h>

char *DEFAULT_STATE_FILENAME = "life.txt";
const char *DEFAULT_OUTPUT_FILENAME = "output.txt";

const int DEFAULT_WIDTH = 10;
const int DEFAULT_HEIGHT = 10;
const int DEFAULT_ITERATIONS = 10;

const int CELL_DEAD = 0;
const int CELL_ALIVE = 1;

const char CELL_ALIVE_CHAR = '*';
const char CELL_DEAD_CHAR = '-';


int getCellState(int *lifeState, int x, int y, int width, int height) {
	if (x < 0 || x >= width || y < 0 || y >= height)
		return CELL_DEAD;
	return lifeState[y * height + x];
}


int getNumAliveNeighbors(int *lifeState, int x, int y, int width, int height) {
	size_t cellsToCheck = 8;
	int dx[] = {-1, 0, 1, -1, 1, -1, 0, 1};
	int dy[] = {-1, -1, -1, 0, 0, 1, 1, 1};

	int neighbors = 0;
	for (size_t i = 0; i < cellsToCheck; ++i) {
		// Alive is 1, dead is 0. Therefore we can get the # of alive
		// neighbors simply by summing their states.
		neighbors += getCellState(lifeState, x + dx[i], y + dy[i], width, height);
	}

	return neighbors;
}


void gameOfLifeUpdate(int *lifeState, int *nextState, int width, int height) {
	int aliveNeighbors;
	for (int y = 0, i = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x, ++i) {
			aliveNeighbors = getNumAliveNeighbors(lifeState, x, y, width, height);
			if ((lifeState[i] && (aliveNeighbors == 2 || aliveNeighbors == 3)) ||
					((!lifeState[i]) && (aliveNeighbors == 3))) {
				nextState[i] = CELL_ALIVE;
			} else {
				nextState[i] = CELL_DEAD;
			}
		}
	}
}


int loadLifeState(FILE *filePointer, int *lifeState, int width, int height) {
	// Read the life grid

	int x = 0;
	int y = 0;
	char c = '\0';

	while ((c = fgetc(filePointer)) != EOF && y < height) {
		if (c == '*') {
			lifeState[y * width + x] = CELL_ALIVE;
		}

		// We are done with this cell. Update!
		++x;

		// TODO: What if the input line is longer than the cell width?
		// Consume all remaining items on this row...
		if (x >= width) {
			while (c != '\n')
				c = fgetc(filePointer);
		}

		if (c == '\n') {
			++y;
			x = 0;
		}
	}

	// Normal execution
	return 0;
}

// Grids are stored flat as a 1D array of ints.
int printLifeState(FILE *filePointer, int *gameState, int width, int height) {
	// Unwrap the grid into 2D.
	for (int i = 0, y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x, ++i) {
			fprintf(filePointer, "%c", gameState[i] ? CELL_ALIVE_CHAR : CELL_DEAD_CHAR);
		}
		fprintf(filePointer, "\n");
	}
	return 0;
}


void swapPointers(int **a, int **b) {
	int *temp = *a;
	*a = *b;
	*b = temp;
}


int overwriteArgumentsFromCommandline(
	int *width,
	int *height,
	int *iterations,
	char **filename,
	int argc,
	char **argv) {

	int newWidth = *width;
	int newHeight = *height;
	int newIterations = *iterations;
	char *newFilename = *filename;

	// Used in tokenizing when checking if user gave us valid input
	char *after;

	// Expected arguments values:
	// 0: executable name
	// 1: width
	// 2: height
	// 3: # iterations
	// 4: filename

	if (argc > 5) {
		fputs("Too many arguments.\n", stderr);
		return -1;
	}

	// Since we are always given the executable name, if that is the
	// only argument we get, we are done.
	if (argc < 2)
		return 0;

	// Can't specify only width w/o specifying height
	if (argc == 2) {
		fputs("Must specify both width and height.\n", stderr);
		return -1;
	}
	
	// Make sure that the user gave us ints
	newWidth = strtol(argv[1], &after, 10);
	if (after[0] != '\0') {
		fputs("Width must be an int.\n", stderr);
		return 1;
	}

	newHeight = strtol(argv[2], &after, 10);
	if (after[0] != '\0') {
		fputs("Height must be an int.\n", stderr);
		return 1;
	}

	// User specified the # of iterations
	if (argc > 3) {
		newIterations = strtol(argv[3], &after, 10);
		if (after[0] != '\0') {
			fputs("# of iterations must be an int.\n", stderr);
			return 1;
		}
	}

	// User specified file name
	if (argc > 4) {
		newFilename = argv[4];
	}

	// Everything is good, overwrite old values
	*width = newWidth;
	*height = newHeight;
	*iterations = newIterations;
	*filename = newFilename;

	return 0;
}


int main(int argc, char **argv) {
	const char *divider = "================================\n";
	
	int width = DEFAULT_WIDTH;
	int height = DEFAULT_HEIGHT;
	int iterations = DEFAULT_ITERATIONS;
	char *filename = DEFAULT_STATE_FILENAME;

	if (overwriteArgumentsFromCommandline(&width, &height, &iterations, &filename, argc, argv) != 0) {
		return 1;
	}

	FILE *outputFilePointer = fopen(DEFAULT_OUTPUT_FILENAME, "w");
	FILE *stateFilePointer = fopen(filename, "r");

	// Make sure file was opened successfully
	if (!stateFilePointer) {
		fprintf(stderr, "Could not open state file: %s\n", filename);
		return 1;
	}

	int *lifeState = calloc(sizeof(int), width * height);
	int *nextState = calloc(sizeof(int), width * height);

	loadLifeState(stateFilePointer, lifeState, width, height);
	fclose(stateFilePointer);

	for (int i = 0; i <= iterations; ++i) {
		fprintf(outputFilePointer, "Generation %d:\n", i);
		printLifeState(outputFilePointer, lifeState, width, height);
		fputs(divider, outputFilePointer);

		gameOfLifeUpdate(lifeState, nextState, width, height);
		swapPointers(&lifeState, &nextState);
	}

	fclose(outputFilePointer);

	// Cleanup anything dynamically allocated in this function
	free(lifeState);
	free(nextState);

	return 0;
}
