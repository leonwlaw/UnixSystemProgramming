/*
 * Program name: du
 * Programmer: Leonard Law (#0428512)
 *
 * Purpose: 
 *   Display disk usage of a directory.
 *   Takes in one optional argument, that changes which directory is being used.
 */

#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>

const int STAT_FAILED = 1;
const int OPENDIR_FAILED = 2;
const int MEMORY_ALLOC_FAILED = 3;

/* Calculates the disk usage for the specified directory.
 */
int diskUsage(char *directoryPath, bool displayOutput) {
  // Now check each file entry...
  DIR *directory;
  if ((directory = opendir(directoryPath)) == NULL) {
    perror("du");
    exit(OPENDIR_FAILED);
  }

  unsigned total = 0;
  for (struct dirent *directoryEntry = readdir(directory);
       directoryEntry != NULL;
       directoryEntry = readdir(directory)) {

    // The parent directory does not play a role in this directory's
    // disk usage.
    if (strcmp("..", directoryEntry->d_name) != 0) {

      // Need to account for terminating \0, as well as path separator.
      size_t directoryPathLength = strlen(directoryPath);
      size_t childNameLength = strlen(directoryEntry->d_name);
      char *childPath;
      if ((childPath = (char *)calloc(
        directoryPathLength + childNameLength + 2, 
        sizeof(char))) == NULL) {

        perror("du");
        exit(MEMORY_ALLOC_FAILED);
      }
      strncpy(childPath, directoryPath, directoryPathLength);
      childPath[directoryPathLength] = '/';
      strncpy(childPath + directoryPathLength + 1, directoryEntry->d_name, childNameLength);

      // Retrieve directory information for the current directory
      struct stat dirstat;
      if (lstat(childPath, &dirstat) != 0) {
        perror("du");
        exit(STAT_FAILED);
      }

      // Each block is 512B large. Therefore, if we divide the # of blocks
      // by 2, we get the size in KB.
      unsigned size = (int)dirstat.st_blocks / 2;

      // Only display if this is a directory, that isn't the current
      // working directory.
      if ((strcmp(".", directoryEntry->d_name) != 0) &&
          ((dirstat.st_mode & S_IFMT) == S_IFDIR)) {
        size = diskUsage(childPath, false);
        if (displayOutput) {
          fprintf(stdout, "%-8d%s\n", size, childPath);
        }
      }
      total += size;
    }
  }
  if (displayOutput) {
    fprintf(stdout, "%-8d%s\n", total, ".");
  }
  return total;
}



int main(int argc, char **argv) {
  char directoryPath[] = ".";
  diskUsage(directoryPath, true);
  return 0;
}

