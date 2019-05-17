#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
int main()
{
    int fd;
    //int n = 10;
    //scanf("%d",&n); 
    //while(n--){ 
    fd  = open("mulu/test.txt",O_RDONLY);
    if(fd<0)
    {
        printf("errno:%d\n",errno);
        exit(0);
    }
    printf("hello word!\n");
    close(fd);
   // }
    return 0;
}

