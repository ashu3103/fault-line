#include <stdlib.h>
#include <fl.h>
#include <page.h>
#include <print.h>


int main()
{
    void* addr = malloc(sizeof(char)*2000);
    print("addr: %a\n", addr);
    void* addr2 = malloc(sizeof(char)*2000);
    print("addr 2: %a\n", addr2);
    void* addr3 = malloc(sizeof(char)*2000);
    print("addr 3: %a\n", addr3);

    print("CHECK FROM HERE!!\n");
    free(addr);
    free(addr2);
    return 0;
}