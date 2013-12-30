

#include <netinet/in.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>




#define max(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })

int do_open(char * file)
{
    int fd;
    fd = open(file, 0);
    if (fd < 0) {
        printf("===== can't open %s\n", file);
        exit(1);
    }
    return fd;
}

int main(int argc, char ** argv)
{
    int fd0,fd1,fd2, max_fd; 
    fd_set max_set, f_set;
    int ret;
    char buf[200];
    int n;

    fd0 = do_open("/dev/select_char0");
    fd1 = do_open("/dev/select_char1");
    fd2 = do_open("/dev/select_char2");


    FD_ZERO(&max_set);
    FD_ZERO(&f_set);

    FD_SET(fd0, &max_set);
    FD_SET(fd1, &max_set);
    FD_SET(fd2, &max_set);

    max_fd = max(max(fd0, fd1), fd2) +1;

    while (1) {
        f_set = max_set;

        ret = select(max_fd, &f_set, NULL, NULL, NULL);
        printf("======%d\n", ret);
        if (FD_ISSET(fd0, &f_set) > 0) {
            n = read(fd0, buf, 200);    
            printf("==read from fd0===%s\n", buf);
        }
        if (FD_ISSET(fd1, &f_set) > 0) {
            n = read(fd1, buf, 200);    
            printf("==read from fd1===%s\n", buf);
        }
        if (FD_ISSET(fd2, &f_set) > 0) {
            n = read(fd2, buf, 200);    
            printf("==read from fd2===%s\n", buf);
        }
    }

    return 0;
}
