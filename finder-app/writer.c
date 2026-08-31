// writer.c

#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <syslog.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        printf("Requires 2 parameters.\n");
        printf("USAGE: writer <file-path> <contents>\n");
        printf("\t<file-path> is the full path to a file to be written\n");
        printf("\t<contents> is the text to be written to the file\n");
        return 1;
    }

    openlog("writer", 0, LOG_USER);

    // argv[0] is the program name or command
    const char* const filePath = argv[1];
    const char* const contents = argv[2];

    // You do not need to make your "writer" utility create directories which
    // do not exist.  You can assume the directory is created by the caller.

    int fd = open(filePath, O_WRONLY | O_CREAT | O_TRUNC);
    if (fd == -1)
    {
        const char* const errorDescription = strerror(errno);
        fprintf(stderr, "Could not open file: %s [%s]\n", errorDescription, filePath);
        syslog(LOG_ERR, "Could not open file: %s [%s]", errorDescription, filePath);
        return 1;
    }

    syslog(LOG_DEBUG, "Writing %s to %s", contents, filePath);

    // Assuming 8-bit characters for the contents passed.
    size_t bytesToWrite = strlen(contents);
    ssize_t bytesWritten = write(fd, contents, bytesToWrite);
    if (bytesWritten == -1)
    {
        const char* const errorDescription = strerror(errno);
        fprintf(stderr, "Could not write file: %s\n", errorDescription);
        syslog(LOG_ERR, "Could not write file: %s", errorDescription);
        return 1;
    }

    if (close(fd) == -1)
    {
        const char* const errorDescription = strerror(errno);
        fprintf(stderr, "Could not close file: %s\n", errorDescription);
        syslog(LOG_ERR, "Could not close file: %s", errorDescription);
        return 1;
    }

    if (chmod(filePath, 0600) == -1)
    {
        const char* const errorDescription = strerror(errno);
        fprintf(stderr, "Could not set file permissions: %s\n", errorDescription);
        syslog(LOG_ERR, "Could not set file permissions: %s", errorDescription);
        return 1;
    }

    return 0;
}

/*

Assignment 1 instructions:
==========================

10) Write a shell script finder-app/writer.sh as described below

Accepts the following arguments: the first argument is a full path to a file
(including filename) on the filesystem, referred to below as writefile; the second
argument is a text string which will be written within this file, referred to below
as writestr

Exits with value 1 error and print statements if any of the arguments above were not
specified

Creates a new file with name and path writefile with content writestr, overwriting any
existing file and creating the path if it doesn’t exist. Exits with value 1 and error
print statement if the file could not be created.

Assignment 2 instructions:
==========================

3. Write a C application “writer” (finder-app/writer.c)  which can be used as an
alternative to the “writer.sh” test script created in assignment1 and using File IO
as described in LSP chapter 2.  See the Assignment 1 requirements for the writer.sh
test script and these additional instructions:

One difference from the write.sh instructions in Assignment 1:  You do not need to make
your "writer" utility create directories which do not exist.  You can assume the directory
is created by the caller.

Setup syslog logging for your utility using the LOG_USER facility.

Use the syslog capability to write a message “Writing <string> to <file>” where <string>
is the text string written to file (second argument) and <file> is the file created by
the script.  This should be written with LOG_DEBUG level.

Use the syslog capability to log any unexpected errors with LOG_ERR level.

Original assignment 1 script:
=============================
#!/bin/bash

if [ $# -ne 2 ]; then
    echo Requires 2 parameters
    exit 1
fi

writefile=$1
writestr=$2

mkdir -p "$(dirname "$writefile")"
if [ $? -ne 0 ]; then
    echo Could not create the directory.
    exit 1
fi

echo "$writestr" > $writefile
if [ $? -ne 0 ]; then
    echo Could not write file $writefile.
    exit 1
fi
EOF

*/
