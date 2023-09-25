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

#define ECHO_PORT 9999
#define BUF_SIZE 40960

#define INFO 1
#define WARNNING 2
#define BUG 3

#define GET 1
#define POST 2
#define HEAD 3

#define ERROR -1
#define NOTHING 0

int err_ret = 0;

const char err_400[] = "HTTP/1.1 400 Bad request\r\n\r\n";
const char err_404[] = "HTTP/1.1 404 Not Found\r\n\r\n";

const char err_501[] = "HTTP/1.1 501 Not Implemented\r\n\r\n";
const char err_505[] = "HTTP/1.1 505 HTTP Version not supported\r\n\r\n";

const char ret_head[] = "HTTP/1.1 200 OK\r\nServer: liso/1.0\r\nDate: Sun, 14 May 2023 07:28:08 UTC\r\nContent-Length: 802\r\nContent-type: text/html\r\nLast-modified: Thu, 24 Feb 2022 10:20:31 GMT\r\nConnection: keep-alive\r\n\r\n";

//这里将BUF作为全局变量使用，方便POST-echo，后续的架构调整中可能需要改动或是使用二级指针
char buf[BUF_SIZE];

void createLog(int level ,char log[])
{
    if(level == BUG) 
        printf("BUG: %s\n",log);
    else if(level == WARNNING) 
        printf("WARNNING: %s\n",log);
    else if(level == INFO)
        printf("INFO: %s\n",log);
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

//处理请求的封装函数
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
    int sock, client_sock;
    ssize_t readret;
    socklen_t cli_size;
    struct sockaddr_in addr, cli_addr;


    fprintf(stdout, "----- Echo Server ----- V0.2.2\n");

    if ((sock = socket(PF_INET, SOCK_STREAM, 0)) == -1)
    {
        fprintf(stderr, "Failed creating socket.\n");
        return EXIT_FAILURE;
    }

    addr.sin_family = AF_INET;
    addr.sin_port = htons(ECHO_PORT);
    addr.sin_addr.s_addr = INADDR_ANY; 

    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)))
    {
        close_socket(sock);
        fprintf(stderr, "Failed binding socket.\n");
        return EXIT_FAILURE;
    }

    if (listen(sock, 5))
    {
        close_socket(sock);
        fprintf(stderr, "Error listening on socket.\n");
        return EXIT_FAILURE;
    }


    while (1)
    {
        cli_size = sizeof(cli_addr);

        if ((client_sock = accept(sock, (struct sockaddr *)&cli_addr,
                                  &cli_size)) == -1)
        {
            close(sock);
            fprintf(stderr, "Error accepting connection.\n");
            return EXIT_FAILURE;
        }

        readret = 0;
        
        memset(buf, 0, BUF_SIZE);
        
        while ((readret = recv(client_sock, buf, BUF_SIZE, 0)) >0)
        {   
            createLog(INFO,"parsing starts .\n");
            Request* request = parse(buf, readret, client_sock);
            createLog(INFO,"parsing finished.\n");

            char* response = processRequest(request);
            
            send(client_sock, response, strlen(response), 0);
    	    memset(buf, 0, BUF_SIZE);
            createLog(INFO,"send finished.\n");
            
            if(request != NULL)
            {
                free(request->headers);
    		    free(request);
            }

            free(response);

            createLog(INFO,"Finished free malloc.\n");
        }

        if (readret == -1)
        {
            close_socket(client_sock);
            close_socket(sock);
            fprintf(stderr, "Error reading from client socket.\n");
            return EXIT_FAILURE;
        }

        if (close_socket(client_sock))
        {
            close_socket(sock);
            fprintf(stderr, "Error closing client socket.\n");
            return EXIT_FAILURE;
        }

        createLog(INFO,"Connection end.\n");
    }

    close_socket(sock);

    return EXIT_SUCCESS;
}
