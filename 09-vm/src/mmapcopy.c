#include "csapp.h"

/*
 * mmapcopy - uses mmap to copy file fd to stdout
 */
void mmapcopy(int fd, int size)
{
    char *bufp; /* ptr to memory-mapped VM area */

    bufp = Mmap(NULL, size, PROT_READ, MAP_PRIVATE|MAP_ANON, 0, 0);
    Write(fd, bufp, size);
    return;
}

/* mmapcopy driver */
int main(int argc, char const *argv[])
{
    struct stat stat;
    int fd;

    /* Check for required command-line argument */
    if (argc != 2) {
        printf("usage: %s <filename>\n", argv[0]);
        return 0;
    }

    /* Copy the input argument to stdout */
    fd = Open(argv[1], O_RDONLY, 0);
    fstat(fd, &stat);
    mmapcopy(fd, stat.st_size);
    return 0;
}
