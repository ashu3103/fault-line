#include <stdlib.h>
#include <fl.h>
#include <page.h>
#include <print.h>


int main()
{
    void* addr = malloc(sizeof(char)*100);
    print("addr: %a\n", addr);
    void* addr2 = malloc(sizeof(char)*200);
    print("addr 2: %a\n", addr2);
    return 0;
}