#include <alloca.h>
#include <stdio.h>
#include <stdlib.h>


int main() {
    int a = 10;

    int *b;
    {
        b = alloca(sizeof(*b));
    }

    printf("aaaaa: %d\n", *b);
    return a;
}
