#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

int main()
{
    printf("Hello World");

    if (fork() == 0) // child
    {
        printf("Hello");
        _exit(0);
    }
    wait(NULL);
}