/*
 * Program name: du
 * Programmer: Leonard Law (#0428512)
 *
 * Purpose: 
 *   Display disk usage of a directory.
 *   Takes in one optional argument, that changes which directory is being used.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>

const int STAT_FAILED = 1;
const int OPENDIR_FAILED = 2;

/* Calculates the disk usage for the specified directory.
 */
int diskUsage(char *directoryPath) {
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

      // Retrieve directory information for the current directory
      struct stat dirstat;
      if (lstat(directoryEntry->d_name, &dirstat) != 0) {
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
        fprintf(stdout, "%-8d./%s\n", size, directoryEntry->d_name);
      }
      total += size;
    }
  }
  fprintf(stdout, "%-8d%s\n", total, ".");
  return 0;
}



int main(int argc, char argv) {
  char directoryPath[] = ".";
  diskUsage(directoryPath);
  return 0;
}

