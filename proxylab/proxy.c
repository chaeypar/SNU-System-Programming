#include <stdio.h>
#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static const char *http_msg = "HTTP/1.0\r\n";
static const char *host_msg = "Host: ";
static const char *connection_msg = "Connection: close\r\n";
static const char *proxyconnection_msg = "Proxy-Connection: close\r\n";

/* Helper functions */
void *thread_func(void *vargp);
void thread_assist(int connfd);
void parse_uri(char *uri, char *host, char *portn, char *detailed);

/* main */
int main(int argc, char **argv) {
    struct sockaddr_in clientaddr;
    pthread_t tid; 

    int clientlen = sizeof(clientaddr);
    int listenfd = Open_listenfd(argv[1]);
    while(1){
        // to avoid unintentional sharing
        int *connfdp = (int *)Malloc(sizeof(int));
        *connfdp = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        // to handle multiple concurrent requests
         Pthread_create(&tid, NULL, thread_func, connfdp);
    }
    return 0;
}

/* thread_func : handle one request */
void *thread_func(void *vargp){
    int connfd = *((int *)vargp);
    // run in detached mode to avoid fatal memory leak
    Pthread_detach(Pthread_self());
    // to close parent's connfd (threaded program)
    Free(vargp);
    thread_assist(connfd);
    Close(connfd);
    return NULL;
}

/* thread_assist: read and parse requests, forward requests to web servers, 
 * read the servers' responses and forward those responses to clients */
void thread_assist(int connfd){
    int n, serverfd;
    char buf[MAXLINE], buf2[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char host[MAXLINE], portn[20], detailed[MAXLINE];
    rio_t rio, rio2;

    // to initialize struct rio_t and readline from it
    Rio_readinitb(&rio, connfd);
    if (!Rio_readlineb(&rio, buf, MAXLINE))
        return;

    // e.g. If "GET http://localhost:12345/home.html HTTP/1.1" is stored in buf,
    // method = "GET", uri = "http://localhost:12345/home.html", version = 'HTTP/1.1'
    sscanf(buf, "%s %s %s", method, uri, version);

    // Only 'GET' method is implemented in this lab
    if (strcasecmp(method, "GET")){
        printf("%s is not implemented\n", method);
        return;
    }

    // parse uri "http://localhost:12345/home.html" into host('localhost'), portn('12345'), and detailed('/home.html')
    parse_uri(uri, host, portn, detailed);
    
    // to create connection with server and initialize struct rio_t
    serverfd = Open_clientfd(host, portn);
    Rio_readinitb(&rio2, serverfd);

    // to write 'GET /home.html HTTP/1.0' in the server
    sprintf(buf2, "%s %s %s", "GET", detailed, http_msg);
    Rio_writen(serverfd, buf2, strlen(buf2));

    // to send http requests header to the server
    int flagu = 0, flaga = 0, flagc = 0, flagpc = 0;
    while((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0){
        // end of line
        if (!strncmp(buf, "\r\n", 2)){
            Rio_writen(serverfd, buf, strlen(buf));
            break;
        }
        // host
        else if (!strncasecmp(buf, "Host", 4)){
            flagu = 1;
            Rio_writen(serverfd, buf, strlen(buf));  
        }
        // user-agent
        else if (!strncasecmp(buf, "User-Agent", 10)){
            flaga = 1;
            Rio_writen(serverfd, user_agent_hdr, strlen(user_agent_hdr));
        }
        // connection
        else if (!strncasecmp(buf, "Connection", 10)){
            flagc = 1;
            Rio_writen(serverfd, connection_msg, strlen(connection_msg));
        }
        // proxy-connection
        else if (!strncasecmp(buf, "Proxy-Connection", 15)){
            flagpc = 1;
            Rio_writen(serverfd, proxyconnection_msg, strlen(proxyconnection_msg));
        }
        // other headers
        else{
            Rio_writen(serverfd, buf, strlen(buf));
        }
    }
    // send 'Host' request header if didn't
    if (!flagu){
        char temp[20];
        sprintf(temp, "%s%s\r\n", host_msg, host);
        Rio_writen(serverfd, temp, strlen(temp));
    }
    // send 'User-Agent' request header if didn't
    if (!flaga){
        Rio_writen(serverfd, user_agent_hdr, strlen(user_agent_hdr));
    }
    // send 'Connection' request header if didn't
    if (!flagc){
        Rio_writen(serverfd, connection_msg, strlen(connection_msg));
    }
    // send 'Proxy-Connection' request header if didn't
    if (!flagpc){
        Rio_writen(serverfd, proxyconnection_msg, strlen(proxyconnection_msg));
    }

    // to forward responses from the server to the client
    while((n = Rio_readlineb(&rio2, buf, MAXLINE)) != 0){
        Rio_writen(connfd, buf, n);
    }
    Close(serverfd);
}

/* parse_uri : parse uri into host name, port number and file path */
void parse_uri(char *uri, char *host, char *portn, char *detailed){
    
    char *ptr;

    if (!(ptr = strstr(uri, "http://"))){
        ptr = uri;
        if (*ptr == '/')
            ptr++;
    }
    else
        ptr += 7;

    // host name
    int i = 0;
    while(*ptr != ':' && *ptr != '/'){
        host[i] = *ptr;
        ptr++;
        i++;
    }
    host[i] = 0;

    // port number
    if (*ptr != ':'){
        portn[0] = '8';
        portn[1] = '0';
        portn[2] = 0;
    }
    else{
        i = 0;
        ptr++;
        while (*ptr != '/' && *ptr != ' '){
            portn[i] = *ptr;
            i++;
            ptr++;
        }
        portn[i] = 0;
    }

    // file path   
    i = 0;
    if (*ptr == '/'){
        while(*ptr){
            detailed[i] = *ptr;
            i++;
            ptr++;
        }
    }
    detailed[i] = 0;
}