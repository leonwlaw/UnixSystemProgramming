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
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>

const int STAT_FAILED = 1;
const int OPENDIR_FAILED = 2;

int main(int argc, char *argv) {
  // Now check each file entry...
  DIR *directory;
  if ((directory = opendir(".")) == NULL) {
    perror("du");
    exit(OPENDIR_FAILED);
  }

  for (struct dirent *directoryEntry = readdir(directory);
       directoryEntry != NULL;
       directoryEntry = readdir(directory)) {

    // Retrieve directory information for the current directory
    struct stat dirstat;
    if (lstat(directoryEntry->d_name, &dirstat) != 0) {
      perror("du");
      exit(STAT_FAILED);
    }

    // Each block is 512B large. Therefore, if we divide the # of blocks
    // by 2, we get the size in KB.
    unsigned size = (int)dirstat.st_blocks / 2;
    fprintf(stdout, "%-8d%s\n", size, directoryEntry->d_name);
  }

  return 0;
}

