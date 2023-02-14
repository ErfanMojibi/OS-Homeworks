#include <stdio.h>
#include <sys/resource.h>

int main() {
    struct rlimit lim;
    int err;
    if((err = getrlimit(RLIMIT_STACK, &lim)) == 0){
    printf("stack size: %ld\n", lim.rlim_cur);
    } else {
        printf("error in getting stack limit size with error no: %d\n", err);
    }
    if((err = getrlimit(RLIMIT_NPROC, &lim)) == 0){
    printf("process limit: %ld\n", lim.rlim_cur);
    } else {
        printf("error in getting process limit with error no: %d\n", err);
    }
    if((err = getrlimit(RLIMIT_NOFILE, &lim)) == 0){
    printf("max file descriptors: %ld\n", lim.rlim_cur);
    } else {
        printf("error in file descriptors limit with error no: %d\n", err);
    }
    return 0;
}
