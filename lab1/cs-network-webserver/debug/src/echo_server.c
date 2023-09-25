/******************************************************************************
* echo_server.c                                                               *
*                                                                             *
* Description: This file contains the C source code for an echo server.  The  *
*              server runs on a hard-coded port and simply write back anything*
*              sent to it by connected clients.  It does not support          *
*              concurrent clients.                                            *
*                                                                             *
* Authors: Athula Balachandran <abalacha@cs.cmu.edu>,                         *
*          Wolf Richter <wolf@cs.cmu.edu>                                     *
*                                                                             *
*******************************************************************************/

#include <netinet/in.h>
#include <netinet/ip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <fcntl.h>
#include <sys/types.h>
#include "../include/parse.h"

#define ECHO_PORT 9999
#define BUF_SIZE 4096

int sock, client_sock;
    ssize_t readret;
    socklen_t cli_size;
    struct sockaddr_in addr, cli_addr;
    char buf[BUF_SIZE];

void echo(char *buf ,ssize_t readret,int sock)
{
    char err_send[] = "HTTP/1.1 400 Bad request\r\n\r\n";
    char g_method[] = "GET";
    char p_method[] = "POST";  
    char err_method[] = "HTTP/1.1 501 Not Implemented\r\n\r\n";


    Request *request = parse(buf,readret,sock);
    if(request == NULL)
    {
        
        send(client_sock, err_send, strlen(err_send), 0);
        return;

    }
        printf("Http Method %s\n",request->http_method);
        printf("Http Version %s\n",request->http_version);
        printf("Http Uri %s\n",request->http_uri);
        printf("Http Connection %s\n",request->http_connection);
    for(int index = 0;index < request->header_count;index++)
    {
        printf("Request Header\n");
        printf("Header name %s Header Value %s\n",request->headers[index].header_name,request->headers[index].header_value);
    }

    if(strcmp(request->http_method,g_method) == 0 || strcmp(request->http_method,p_method)==0)
    {
        send(client_sock, buf, readret, 0);
    }
    else
    {
        send(client_sock, err_method, readret, 0);
    }

    free(request->headers);
    free(request);
    
}


int close_socket(int sock)
{
    if (close(sock))
    {
        printf("Failed closing socket.\n");
        return 1;
    }
    return 0;
}

int main(int argc, char* argv[])
{
    // int sock, client_sock;
    // ssize_t readret;
    // socklen_t cli_size;
    // struct sockaddr_in addr, cli_addr;
    // char buf[BUF_SIZE];

    printf("----- Echo Server V0.0.4-----\n");
    
    /* all networked programs must create a socket */
    if ((sock = socket(PF_INET, SOCK_STREAM, 0)) == -1)
    {
        printf("Failed creating socket.\n");
        return EXIT_FAILURE;
    }

    addr.sin_family = AF_INET;
    addr.sin_port = htons(ECHO_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    /* servers bind sockets to ports---notify the OS they accept connections */
    if (bind(sock, (struct sockaddr *) &addr, sizeof(addr)))
    {
        close_socket(sock);
        printf("Failed binding socket.\n");
        return EXIT_FAILURE;
    }


    if (listen(sock, 5))
    {
        close_socket(sock);
        printf("Error listening on socket.\n");
        return EXIT_FAILURE;
    }

    /* finally, loop waiting for input and then write it back */
    while (1)
    {
        cli_size = sizeof(cli_addr);
        client_sock = accept(sock, (struct sockaddr *) &cli_addr,&cli_size);
        
        if (client_sock == -1)
        {
            close(sock);
            printf("Error accepting connection.\n");
            return EXIT_FAILURE;
        }
        else
        {
            printf("-New connection\n");
        }

        readret = 0;
        readret = recv(client_sock, buf, BUF_SIZE, 0);

        while(readret  > 0)
        {
            //这里的send就是重复内容
            //size_t echo_ret = send(client_sock, buf, readret, 0);
            printf("--send-echo:%s\n",buf);

            ////////////////////////////
            //size_t echo_ret = 
            echo(buf,readret,sock);
                
            ////////////////////////////

            // if ( echo_ret!= readret)
            // {
            //     close_socket(client_sock);
            //     close_socket(sock);
            //     printf("Error sending to client.\n");
            //     return EXIT_FAILURE;
            // }
            
            //注意下次读取是否会不会自动置零
            memset(buf, 0, BUF_SIZE);
            readret = recv(client_sock, buf, BUF_SIZE, 0);

        }

        printf("connention end\n");

        if (readret == -1)
        {
            close_socket(client_sock);
            close_socket(sock);
            printf("Error reading from client socket.\n");
            return EXIT_FAILURE;
        }

        int close_code = close_socket(client_sock);
        if (close_code)
        {
            close_socket(sock);
            printf("Error closing client socket.\n");
            return EXIT_FAILURE;
        }

        printf("listening turned \n");
    }

    printf("-close,exiting\n");
    close_socket(sock);
    

    return EXIT_SUCCESS;
}
