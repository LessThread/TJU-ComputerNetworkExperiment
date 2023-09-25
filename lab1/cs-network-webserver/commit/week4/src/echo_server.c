#include <netinet/in.h>
#include <netinet/ip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <errno.h>
#include "parse.h"
#include <time.h>

#define ECHO_PORT 9999
#define BUF_SIZE 40960

//全局宏定义
#define debugger getchar();printf("debugger\n")
//错误等级
#define INFO 1
#define WARNNING 2
#define BUG 3

//请求方法
#define OK 0
#define GET 1
#define POST 2
#define HEAD 3

//返回码
#define ERROR -1
#define NOTHING 0

//系统全局错误码，未使用
int err_ret = 0;

const char err_400[] = "HTTP/1.1 400 Bad request\r\n\r\n";
const char err_404[] = "HTTP/1.1 404 Not Found\r\n\r\n";

const char err_501[] = "HTTP/1.1 501 Not Implemented\r\n\r\n";
const char err_505[] = "HTTP/1.1 505 HTTP Version not supported\r\n\r\n";

const char ret_head[] = "HTTP/1.1 200 OK\r\nServer: liso/1.0\r\nDate: Sun, 14 May 2023 07:28:08 UTC\r\nContent-Length: 802\r\nContent-type: text/html\r\nLast-modified: Thu, 24 Feb 2022 10:20:31 GMT\r\nConnection: keep-alive\r\n\r\n";


//获取local time
char* getTime()
{
    char* str= (char*)malloc(sizeof(char)*BUF_SIZE); 

    time_t timer;
    struct tm *Now;
    time( &timer );
    Now = localtime( &timer );
    //printf("%s\n", asctime(Now));
    strcpy(str,asctime(Now));
    
    return str;
}

//封装日志系统，为后来设计留接口
void createLog(int level ,char log[])
{
    FILE *f = fopen("./log.txt", "a+");
    char logs[BUF_SIZE]={0};
    char* time_str = NULL;
    if(level == BUG)
    {
        printf("\033[0;31;40m[BUG]\033[0m %s\n",log);
        time_str = getTime();
        strcpy(logs,time_str);
        strcat(logs," [BUG] ");
        strcat(logs,log);
        fputs(logs, f);
    }
    else if(level == WARNNING) 
    {
        printf("\033[0;33;40m[WARNNING]\033[0m %s\n",log);
        time_str = getTime();
        strcpy(logs,time_str);
        strcat(logs," [WARNNING] ");
        strcat(logs,log);
        fputs(logs, f);
    }
    else if(level == INFO)
    {
        printf("\033[0;34;40m[INFO]\033[0m %s\n",log);
        time_str = getTime();
        strcpy(logs,time_str);
        strcat(logs," [INFO] ");
        strcat(logs,log);
        fputs(logs, f);
    }
    free(time_str);
    
    
    fclose(f);
}



//接收缓冲区,
char all_request_buf[BUF_SIZE*10];

//这里将BUF作为全局变量使用，方便POST-echo，后续的架构调整中可能需要改动函数参数或是使用二级指针
char buf[BUF_SIZE];

int end_index = 0;
int pass_index = 0;

//消息预处理模块V0.1，分割多个请求(手写)
int splittingMessage()
{
    createLog(INFO,"start splitting\n");
    char sp[4] = {0};
    memset(buf, 0, BUF_SIZE);

    for(int i = 0;end_index<(strlen(all_request_buf));end_index++,i++)
    {
        buf[i] = all_request_buf[end_index];
        sp[end_index%4] = all_request_buf[end_index];
        //printf("buf:\n%s\n",buf);

        if(((sp[3] == '\r')&&(sp[2] == '\n')&&(sp[1] == '\r')&&(sp[0] == '\n'))||((sp[0] == '\r')&&(sp[1] == '\n')&&(sp[2] == '\r')&&(sp[3] == '\n')))
        {
            buf[i+1]='\0';
            pass_index = end_index;
            pass_index++;
            end_index++;
            return 1;
        }
    }

    printf("sp ends");
    end_index = 0;
    pass_index = 0;
    return 0;
}

//判断HTTP版本
int determineHttpVersion(char str[])
{
    if(strcmp(str,"HTTP/1.1") == 0)
    {
        createLog(INFO,str);
        return NOTHING;
    }
    else
    {
        createLog(WARNNING,str);
        return ERROR;
    }
}

//判断请求类型
int determineRequestType(char str[])
{
    if(strcmp(str,"GET")==0) return GET;
    if(strcmp(str,"POST")==0) return POST;
    if(strcmp(str,"HEAD")==0) return HEAD;
    return ERROR;
}

//POST方法实现
char* implementPOST(char* response,Request* request)
{
    strcpy(response,buf);
    return response;
}

//HEAD方法实现
char* implementHEAD(char* response,Request* request)
{
    char filename[BUF_SIZE];
    char path[BUF_SIZE] = "static_site";
    strcpy(filename,request->http_uri);
    
    //字符串处理
    if(strcmp(filename,"/") == 0)
        strcpy(filename, "/index.html");
    strcat(path,filename);

    //404返回 //************************************************************
    struct stat st;
    if(stat(path,&st) == -1)
    {
        //err_ret = 1;
        strcpy(response,err_404);
        return response;
    }

    strcpy(response,ret_head);
    return response;
}

//GET方法实现
char* implementGET(char* response,Request* request)
{
    char filename[BUF_SIZE];
    char path[BUF_SIZE] = "static_site";
    strcpy(filename,request->http_uri);

    //字符串处理
    if(strcmp(filename,"/") == 0)
        strcpy(filename, "/index.html");
    strcat(path,filename);

    //404返回
    struct stat st;
    if(stat(path,&st) == -1)
    {
        //err_ret = 1;
        strcpy(response,err_404);
        return response;
    }


    //文件返回
    char* fbuff = (char*)malloc(sizeof(char)* BUF_SIZE);
    FILE *fp = NULL;
    fp = fopen(path, "r");
    if(fp == NULL)
    {
        strcpy(response,err_404);
        return response;
    }
    fseek(fp,0,SEEK_END);
    long lSize = ftell(fp);
    rewind(fp);
    fread(fbuff,sizeof(char),lSize,fp);
    fbuff[lSize] = '\0';

    printf("%s:%s\n",path ,fbuff);
    printf("%d:%s\n",lSize, fbuff);
    
    strcpy(response,ret_head);
    strcat(response,fbuff);
    free(fbuff);


    return response;

}

//处理请求的分发封装函数
char* processRequest(Request* request)
{
    char* response = (char*) malloc(sizeof(char) * BUF_SIZE);
    if (request == NULL)
    {
        strcpy(response,err_400);
        //err_ret = 1;
        return response;
    }


    if(determineHttpVersion(request->http_version) == ERROR)
    {
        strcpy(response,err_505);
        //err_ret = 1;
        return response;
    }

    int RequestType = determineRequestType(request->http_method);
    if( RequestType == ERROR)
    {
        strcpy(response,err_501);
        //err_ret = 1;
        return response;
    }

    switch(RequestType)
    {
        case(GET): {response = implementGET(response,request);break;}
        case(POST):{response = implementPOST(response,request);break;}
        case(HEAD):{response = implementHEAD(response,request);break;}
        default:{createLog(BUG,"UnCatch Type\n");exit(-1);}
    }

    return response;          
}

int close_socket(int sock)
{
    if (close(sock))
    {
        fprintf(stderr, "Failed closing socket.\n");
        return 1;
    }
    return 0;
}




int main(int argc, char *argv[])
{
    const int MAX_CLIENT_COUNT = 1025;

    int sock, client_socks[MAX_CLIENT_COUNT];
    ssize_t readret;
    socklen_t cli_size;
    struct sockaddr_in addr, cli_addr;




    fprintf(stdout, "----- Echo Server V0.4.3 Powered By C-----\n");

    if ((sock = socket(PF_INET, SOCK_STREAM, 0)) == -1)
    {
        fprintf(stderr, "Failed creating socket.\n");
        return EXIT_FAILURE;
    }

    addr.sin_family = AF_INET;
    addr.sin_port = htons(ECHO_PORT);
    addr.sin_addr.s_addr = INADDR_ANY; 

    while(bind(sock, (struct sockaddr *)&addr, sizeof(addr)))
    {
        //close_socket(sock);
        fprintf(stderr, "Failed binding socket.\n");
        sleep(1);
    }

    printf("binded OK\n");

    if (listen(sock, 5))
    {
        close_socket(sock);
        fprintf(stderr, "Error listening on socket.\n");
        return EXIT_FAILURE;
    }


    //初始化客户端组
    for (int i = 0; i < MAX_CLIENT_COUNT; i++) 
    {
        client_socks[i] = 0;
    }


    //以下是对select函数的初始化设置

    fd_set readfds;
    FD_ZERO(&readfds);

    FD_SET(sock, &readfds);
    int max_sd = sock;

    struct timeval mytime;
    printf("wait for client connnect!\n");

    while (1)
    {

        //定义超时时间（不知道有没有用）
        mytime.tv_sec=27;
        mytime.tv_usec=0;

        FD_ZERO(&readfds);

        //重置监听组（不重置总会在每次循环中报错，尚未知原因，可能是客户端退出导致的）
        if(max_sd < 0)
        {
            max_sd=0;
        }

        //加入服务端socket
        FD_SET(sock,&readfds);
        if(max_sd < sock)
        {
            max_sd = sock;
        }

        //加入客户端socket，修正监听组
        for(int i=0;i<MAX_CLIENT_COUNT;i++) 
        {
            if( client_socks[i]!=0)
            {
                FD_SET( client_socks[i],&readfds);
                if(max_sd <  client_socks[i])
                {
                    max_sd =  client_socks[i];
                }
            }
        }
        


        //select多路复用
        int ret = select(max_sd + 1, &readfds, NULL, NULL, &mytime);
 
        if(ret < 0)   
        {   //对就是这儿报错
            printf("select failure\n");
            continue;   
        }   
 
        else if(ret == 0)
        {
            //超时，还没试过
            printf("time out!");
            continue;
        }
 
        else
        {
            //检查服务端socket可用性
            if(FD_ISSET(sock, &readfds))
            {
                struct sockaddr_in client_address;
                socklen_t address_len;

                int client_sock_fd = accept(sock,(struct sockaddr *)&client_address, &address_len);

                if(client_sock_fd > 0)
                {
                    //分配并记录fd，最多只能有 MAX_CLIENT_COUNT 个客户端
                    for(int i=0;i<MAX_CLIENT_COUNT;i++)
                    {
                        if(client_socks[i] == 0)
                        {
                            printf("new user client[%d] add sucessfully!\n",i);
                            client_socks[i] = client_sock_fd;
                            break;
                        }
                    }
                }   
            }
 
        }

        //处理部分（类似于week3）
        for (int i = 0; i < MAX_CLIENT_COUNT; i++) 
        {
            int client_sock = client_socks[i];
            if(client_sock == 0)
                {continue;}

            if (FD_ISSET(client_sock, &readfds)) 
            {
                memset(buf, 0, BUF_SIZE);
                memset(all_request_buf, 0, BUF_SIZE);
                readret = recv(client_sock, all_request_buf, BUF_SIZE,0);

                if (readret == 0) 
                {
                    //fprintf(stderr, "readret == 0\n");
                    client_socks[i] = 0;
                    FD_CLR(client_socks[i], &readfds);
                    continue;
                    //return EXIT_FAILURE;
                }
                else if (readret == -1)
                {
                    //fprintf(stderr, "readret == -1\n");
                    client_socks[i] = 0;
                    return EXIT_FAILURE;
                }
                else
                {
                    //fprintf(stderr, "splittingMessage()\n");
                    while(splittingMessage())
                    {
                        printf("REQUEST: \n%s\n",buf);

                        Request* request = parse(buf, readret, client_sock);


                        char* response = processRequest(request);
                        
                        send(client_sock, response, strlen(response), 0);
                        memset(buf, 0, BUF_SIZE);
                        
                        //特别注意内存管理和空指针释放
                        if(request != NULL)
                        {
                            free(request->headers);
                            free(request);
                        }

                        free(response);
                    }
                }

                
            }

        }
    }

    close_socket(sock);

    return EXIT_SUCCESS;
}
