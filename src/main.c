#include <stdlib.h>
#include <fl.h>
#include <print.h>


int main()
{
    void* addr = malloc(sizeof(char));
    print("addr: %a\n", addr);
    return 0;
}