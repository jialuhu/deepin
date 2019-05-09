#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <assert.h>
#include <errno.h>

using namespace std;

int setnonblocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

void addfd(int epfd, int fd, bool flag)
{
    epoll_event ev;
    ev.data.fd = fd;
    ev.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    if(flag)
    {
        ev.events = ev.events | EPOLLONESHOT;
    }
    epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);
    setnonblocking(fd);
}

void modfd(int epfd, int fd, int ev)
{
    epoll_event event;
    event.data.fd = fd;
    event.events = ev|EPOLLET|EPOLLONESHOT|EPOLLRDHUP;
    epoll_ctl(epfd,EPOLL_CTL_MOD,fd,&event);
}

int main()
{
    /*创建Unix套接字*/
    struct sockaddr_un myserver;
    int socketfd;
    socketfd = socket(AF_LOCAL, SOCK_STREAM, 0);
    if(socketfd < 0)
    {
        cout << "socket is failt!\n";
        return 0;
    }
    myserver.sun_family = AF_LOCAL;
    strcpy(myserver.sun_path, "/home/hujialu/deepin/s_hook");
    int r = unlink("/home/hujialu/deepin/s_hook");
    cout << "r: " << r << endl;
    bind(socketfd,(struct sockaddr*)&myserver,sizeof(myserver));
    int ret = listen(socketfd,5);
    if(ret < 0)
    {
        cout << ret << endl;
        cout << "listen is failt!\n";
        return 0;
    }
    
    int epfd;
    epoll_event events[1000];
    epfd = epoll_create(5);
    assert(epfd != -1);
    addfd(epfd, socketfd, false);
    while(true)
    {
        int number = epoll_wait(epfd, events, 1000, -1);
        if( (number < 0) && (errno != EINTR) )
        {
            printf("my epoll is failure!\n");
            break;
        }
        for(int i=0; i<number; i++)
        {
            int sockfd = events[i].data.fd;
            if(sockfd == socketfd)//有新用户连接
            {
                struct sockaddr_un client_address;
                socklen_t client_addresslength = sizeof(client_address);
                int client_fd = accept(sockfd,(struct sockaddr*)&client_address, &client_addresslength);
                if(client_fd < 0)
                {
                    printf("errno is %d\n",errno);
                    continue;
                }
                cout << "接受连接成功\n";
                //初始化客户连接
                cout << epfd << " " << client_fd << endl;
                addfd(epfd, client_fd, true);
            }
            else if(events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR))
            {
             //   *出现异常则关闭客户端接/
                cout << "出现异常，关闭进程连接\n";
                //users[sockfd].close_coon();
            }
            else if(events[i].events & EPOLLIN)//可以读取
            {
                cout << "可读取进程\n";
                char buf[245];
                read(sockfd,buf,245);
                buf[strlen(buf)+1]='\0';
                cout << "buf:" << buf << endl;
                modfd(epfd,sockfd,EPOLLOUT);
                /*if(users[sockfd].myread())
                {
                    cout << "kedu\n";
                    pool->addjob(users+sockfd);
                }
                else{
                    users[sockfd].close_coon();
                }*/
            }
            else if(events[i].events & EPOLLOUT)//可写入
            {
                /*if(!users[sockfd].mywrite())
                {
                    users[sockfd].close_coon();
                }*/
                char buf[245];
                sprintf(buf,"OPEN_CALL_OK\r\n");
                buf[strlen(buf)+1] = '\0';
                int w = write(sockfd, buf, strlen(buf));
                cout <<" w: " << w << endl;
                cout << "可以写入信息给进程\n";
            }
        }
    }
    close(epfd);
    close(socketfd);
    std::cout << "Hello world" << std::endl;
    return 0;
}

