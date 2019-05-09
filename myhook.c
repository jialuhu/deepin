#include <stdio.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include "myhook.h"

/*Unix套接字建立、连接*/
enum MONITOR_STATE Unix_Socket(enum TYPE_HOOK types, char *pathname);

/*分析监测系统返回的消息包,返回类型为是否备份成功或者取备份成功*/
enum MONITOR_STATE prase_monitor_package(const char *package);


enum MONITOR_STATE prase_monitor_package(const char *package)
{
    printf("package:%s\n",package);
    return OPEN_SAVE_OK;
}


enum MONITOR_STATE Unix_Socket(enum TYPE_HOOK types, char *pathname)
{
    char buf[245];//与监测系统传递的消息包
    enum TYPE_HOOK type_call = types;//触发hook劫持的系统调用函数类型(open/close)
    enum MONITOR_STATE monitor_state;//监测系统返回的状态
   
    int conn_socket = -1;//Unix套接字初始化
    struct sockaddr_un myaddr;//Unxi套接字结构
    
    /*建立Unix套接字*/
    conn_socket = socket(AF_LOCAL, SOCK_STREAM, 0);
    if(conn_socket == -1)
    {
        return USOCKET_FAILT;
    }
    
    bzero(&myaddr, sizeof(myaddr));
    myaddr.sun_family = AF_LOCAL;
    strcpy(myaddr.sun_path, "/home/hujialu/deepin/s_hook");

    /*建立进程间的通信连接*/
    int ret = connect(conn_socket, (struct sockaddr*)&myaddr, sizeof(myaddr));
    if(ret < 0)
    {
        return UCONNECT_FAILT;
    }
    printf("连接成功\n");
   
    /*根据TYPE_HOOK的方式进行分别填写与监测系统通信的协议包*/
    if(type_call == OPEN_CALL)
    {
        /*open函数调用*/
        sprintf(buf,"OPEN %s\r\n",pathname);
    }
    else{

        /*close函数调用*/
        sprintf(buf, "CLOS %s\r\n",pathname);
    }
    buf[strlen(buf)+1] = '\0';
    printf("%s\n",buf);
    
    /*向监测系统发送函数调用消息*/
    int red = write(conn_socket, buf, strlen(buf));
    if(red<=0)
    {
        printf("write failt!\n");
        return UWRITE_FAILT;
    }
    printf("%d字节已经发送\n",red); 
    
    /*char buff[245];
    int r;
    r=read(conn_socket,buff, 245);
    printf("非阻塞:r=%d\n",r);*/
    /*
     * while(1)
     * {
     *      sleep(1);
     * }
     * */
    /*Unix套接字设置为非阻塞，所以要轮询处理*/ 
    while(1)
    {
        char buff[245];
        int r;
        bzero(buff,245);
        if((r=read(conn_socket,buff, 245)) != 0)
        {
            buff[strlen(buff)+1] = '\0';
            printf("读取到了:%d字节数\n",r);
            printf("读取到监测系统返回的包:%s\n",buff);
            monitor_state = prase_monitor_package(buff);
            break;
        }
    }
    close(conn_socket);
    return monitor_state;
    //return OPEN_SAVE_OK;
}



const char *path="/home/hujialu/myhook/";
typedef int(*OPEN)(const char*, int, ...);
int open(const char *pathname, int flags, ...)
{
    /*判断open的参数是两个还是三个参数*/
    int parameter;
    va_list argptr;
    va_start( argptr, flags );
    mode_t mode = va_arg(argptr,mode_t);
    if (mode >= 0 && mode <= 0777) {
        parameter = 1;
   }
    va_end( argptr );
    
    /*首先获取程序运行的绝对路径，也就是触发open函数调用的程序的绝对目录*/
    char buf[256];
    getcwd(buf,256);
    printf("程序当前运行的目录:%s\n",buf);
   
    /*若为相对路径，则通过程序当前的路径进行绝对路径的补充*/ 
    if(pathname[0]!='/')//说明是相对路径，不是绝对路径
    {
        printf("程序中open文件的路径为:%s\n",pathname);
        sprintf(buf,"%s/%s",buf,pathname);
        buf[strlen(buf)+1]='\0';
        printf("相对路径:%s\n",buf);
    }
   
    /*若为绝对路径，将路径拷贝到buf中*/
    else{
        strcpy(buf, pathname);
        buf[strlen(buf)+1]='\0';
        printf("绝对路径:%s\n",buf);
    }
   
    /*将最后处理后的绝对路径进行过滤，比如过滤掉../以及./这样的路径*/
    char real_path[256];
    realpath(buf, real_path);
    real_path[strlen(real_path)+1] = '\0';
    printf("realpath: %s\n",real_path);
    
    /*打开一个动态链接库*/
    static void *handle = NULL;
    static OPEN old_open = NULL;
    if(!handle)
    {
        handle = dlopen("libc.so.6", RTLD_LAZY);
        old_open = (OPEN)dlsym(handle, "open");
        dlclose(handle);
    }
    
    /*判断是否是监测目录*/
    int file_path_len = strlen(path);//获取监测目录绝对路径的长度
    int ret = strncmp(path, real_path, file_path_len);
    if( ret==0 )//属于监测系统监测的目录,需要进行Unix进程通信处理
    {
        printf("在被监测的该路,需要进行进程间的通信\n");
        
        /*进程间的通信*/
        enum MONITOR_STATE monitor_state;//记录监测系统返回的状态
        monitor_state = Unix_Socket(OPEN_CALL, real_path);
        printf("recv_flag:%d\n",monitor_state);
        
        /*查看监测系统是否正常备份或者取到备份，若备份和取备份失败，将errno设置为无权限操作错误,直接返回-1*/ 
        if( monitor_state == OPEN_SAVE_OK || monitor_state==CLOSE_GET_OK )//监测系统服务器备份修改和取备份修改成功
        {
            if(parameter)//判断是几个参数的系统调用
            {
                return old_open(pathname, flags, mode);
            }
            else{
                return old_open(pathname, flags);
            }
        }
        else{//监测系统备份和取备份失败
                errno = 1;//Operation not permitted
                return -1;
        }
    }
    //不属于监测系统监测的目录,需要立即返回原open函数
    else{

        printf("不在被监测的该路径\n");
        /*进程处理函数，进程处理之后再返回原函数*/
        if(parameter)//有三个参数的open
        {
            return old_open(pathname, flags, mode);
        }
        else{//只有两个参数的open
               printf("my hook of open\n");
               return old_open(pathname, flags);
        }

    }
}


/*int main()
{
    printf("Hello world\n");
    return 0;
}*/

