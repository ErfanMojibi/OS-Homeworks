/* A simple test harness for memory alloction. */

#include "mm_alloc.h"
#include <stdio.h>
int main(int argc, char **argv)
{
    int *data;

    data = (int*) mm_malloc(20);
    data[0] = 1;
    data[1] = 2;
    data[4] = 10;
    printf("%d %d %d %d %d %d\n", data, data[0], data[1], data[2], data[3], data[4]);
    
    mm_free(data);
    data = mm_malloc(20);
    printf("%d %d %d %d %d %d\n", data, data[0], data[1], data[2], data[3], data[4]);
    mm_free(data);
    printf("malloc sanity test successful!\n");
    return 0;
}
