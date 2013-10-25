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
const int CLOSEDIR_FAILED = 3;
const int MEMORY_ALLOC_FAILED = 4;
const int SEEN_INODE_BUFFER_OVERFLOW = 5;

// The size of the temporary array to hold the child path at each iteration.
// 1023 characters should be plenty, I hope.
const size_t CHILD_PATH_BUFFER_SIZE = 4096;

const int SEEN_INODE_BUFFER_SIZE = 1024;


/* Function Stubs
 */

/* Checks to see if the specified inode is in the array.
 */
bool isInInodesArray(ino_t inode, ino_t *seenInodes, size_t length);

/* Calculates the disk usage for the specified directory.
 * Prints out the disk usage for child directories.
 */
int diskUsage(char *directoryPath, ino_t **seenInodes, size_t *lastUsedInodeIndex, size_t seenInodesLength);


int main(int argc, char **argv) {
  char *directoryPath = ".";
  // Necessary to keep track of multiple hard links, so that we don't double count.
  ino_t *seenInodes;
  if ((seenInodes = calloc(SEEN_INODE_BUFFER_SIZE, sizeof(ino_t))) == NULL) {
    perror("du");
    exit(MEMORY_ALLOC_FAILED);
  }
  if (argc > 1) {
    directoryPath = argv[1];
  }
  size_t lastUsedInodeIndex = 0;
  int size = diskUsage(directoryPath, &seenInodes, &lastUsedInodeIndex, SEEN_INODE_BUFFER_SIZE);
  fprintf(stdout, "%-8d%s\n", size, directoryPath);

  // Free up anything we allocated...
  free(seenInodes);
  return 0;
}


/* Function Definitions
 */

int diskUsage(char *directoryPath, ino_t **seenInodes, size_t *lastUsedInodeIndex, size_t seenInodesLength) {
  // Now check each file entry...
  DIR *directory;
  if ((directory = opendir(directoryPath)) == NULL) {
    perror("du");
    exit(OPENDIR_FAILED);
  }

  // Build the path for the child item, so that we can use it
  // to query the inode information from the system.
  // Need to account for terminating \0.
  char *fullPath;
  if ((fullPath = (char *)calloc(CHILD_PATH_BUFFER_SIZE, sizeof(char))) == NULL) {
    perror("du");
    exit(MEMORY_ALLOC_FAILED);
  }

  // This is the path to the current directory.
  // This is common to all child elements.
  size_t directoryPathLength = strlen(directoryPath);
  strncpy(fullPath, directoryPath, directoryPathLength);
  fullPath[directoryPathLength] = '/';

  // Children only need to modify the part of the path after this...
  char *childPath = fullPath + directoryPathLength + 1;

  unsigned total = 0;

  // Check files...
  off_t directoryBeginning = telldir(directory);
  for (struct dirent *directoryEntry = readdir(directory);
       directoryEntry != NULL;
       directoryEntry = readdir(directory)) {

    // Modify the path for the child item, so that we can use it
    // to query the inode information from the system.
    size_t childNameLength = strlen(directoryEntry->d_name);
    // Path separator is in the middle, so +1
    strncpy(childPath, directoryEntry->d_name, childNameLength);
    childPath[childNameLength] = '\0';

    // Retrieve directory information for the current directory
    struct stat dirstat;
    if (lstat(fullPath, &dirstat) != 0) {
      perror("du");
      exit(STAT_FAILED);
    }

    // Only count files.
    if (S_ISREG(dirstat.st_mode)) {
      // Each block is 512B large. Therefore, if we divide the # of blocks
      // by 2, we get the size in KB.
      unsigned size = (int)dirstat.st_blocks / 2;

      if (dirstat.st_nlink == 1) {
        total += size;
      } else if (isInInodesArray(dirstat.st_ino, *seenInodes, seenInodesLength) == false) {
        // This inode has not been seen before
        total += size;

        if (*lastUsedInodeIndex == seenInodesLength) {
          perror("du");
          exit(SEEN_INODE_BUFFER_OVERFLOW);
        }

        // Record this down so we know not to count this again.
        *(*seenInodes + *lastUsedInodeIndex) = dirstat.st_ino;
        ++(*lastUsedInodeIndex);
      }
    }
  }

  seekdir(directory, directoryBeginning);
  for (struct dirent *directoryEntry = readdir(directory);
       directoryEntry != NULL;
       directoryEntry = readdir(directory)) {

    // The parent directory does not play a role in this directory's
    // disk usage.
    if (strcmp("..", directoryEntry->d_name) != 0) {

      // Modify the path for the child item, so that we can use it
      // to query the inode information from the system.
      size_t childNameLength = strlen(directoryEntry->d_name);
      // Path separator is in the middle, so +1
      strncpy(childPath, directoryEntry->d_name, childNameLength);
      childPath[childNameLength] = '\0';

      // Retrieve directory information for the current directory
      struct stat dirstat;
      if (lstat(fullPath, &dirstat) != 0) {
        perror("du");
        exit(STAT_FAILED);
      }

      // Each block is 512B large. Therefore, if we divide the # of blocks
      // by 2, we get the size in KB.
      unsigned size = (int)dirstat.st_blocks / 2;

      // Only display if this is a directory, that isn't the current
      // working directory.
      if ((dirstat.st_mode & S_IFMT) == S_IFDIR) {
        if (strcmp(".", directoryEntry->d_name) != 0) {
          size = diskUsage(fullPath, seenInodes, lastUsedInodeIndex, seenInodesLength);
          fprintf(stdout, "%-8d%s\n", size, fullPath);
        }

        total += size;
      } 
    }
  }
  if (closedir(directory) != 0) {
    perror("du");
    exit(CLOSEDIR_FAILED);
  }

  free(fullPath);
  return total;
}

bool isInInodesArray(ino_t inode, ino_t *seenInodes, size_t length) {
  for (size_t i = 0; i < length; ++i) {
    if (*(seenInodes + i) == inode) {
      return true;
    }
  }
  return false;
}


