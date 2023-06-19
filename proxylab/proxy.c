#include <stdio.h>
#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static const char *get_msg = "Get ";
static const char *http_msg = "HTTP/1.0\r\n";
static const char *host_msg = "Host: ";
static const char *end_msg = "\r\n";
static const char *connection_msg = "Connection: close\r\n";
static const char *proxyconnection_msg = "Proxy-Connection: close\r\n";

void thread_assist(int connfd);
void *thread_func(void *vargp);

int main(int argc, char **argv)
{
    //printf("%s", user_agent_hdr);
    int port = atoi(argv[1]);
    struct sockaddr_in clientaddr;
    pthread_t tid; 

    int clientlen = sizeof(clientaddr);
    int listenfd = Open_listenfd(argv[1]);
    while(1){
        int *connfdp = (int *)Malloc(sizeof(int));
        *connfdp = Accept(listenfd, (SA *)&clientaddr, &clientlen);
         Pthread_create(&tid, NULL, thread_func, connfdp);
    }
    return 0;
}

void *thread_func(void *vargp){
    int connfd = *((int *)vargp);
    Pthread_detach(Pthread_self());
    Free(vargp);
    thread_assist(connfd);
    Close(connfd);
    return NULL;
}


void debug(){

}

void thread_assist(int connfd){
    int n, clientfd;
    char buf[MAXLINE], buf2[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char host[MAXLINE], portn[20], detailed[MAXLINE];
    rio_t rio, rio2;

    Rio_readinitb(&rio, connfd);
    if (!Rio_readlineb(&rio, buf, MAXLINE))
        return;

    sscanf(buf, "%s %s %s", method, uri, version);
    if (strcasecmp(method, "GET")){
        printf("%s is not implemented\n", method);
        return;
    }

    parse_uri(uri, host, portn, detailed);
    
    clientfd = Open_clientfd(host, portn);
    Rio_readinitb(&rio2, clientfd);

    sprintf(buf2, "%s %s %s", "GET", detailed, http_msg);
    printf("%s %s", detailed, http_msg);
    Rio_writen(clientfd, buf2, strlen(buf2));
    
    int flagu = 0, flagc=0, flagpc = 0;
    while((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0){
        if (!strncmp(buf, end_msg, 2)){
            Rio_writen(clientfd, buf, strlen(buf));
            break;
        }
        else if (!strncasecmp(buf, "Host", 4)){
            flagu = 1;
            Rio_writen(clientfd, buf, strlen(buf));  
        }
        else if (!strncasecmp(buf, "User-Agent", 10)){
            Rio_writen(clientfd, user_agent_hdr, strlen(user_agent_hdr));
        }
        else if (!strncasecmp(buf, "Connection", 10)){
            flagc = 1;
            Rio_writen(clientfd, connection_msg, strlen(connection_msg));
        }
        else if (!strncasecmp(buf, "Proxy-connection", 15)){
            flagpc = 1;
            Rio_writen(clientfd, proxyconnection_msg, strlen(proxyconnection_msg));
        }
        else{
            Rio_writen(clientfd, buf, strlen(buf));
        }
    }
    if (!flagu){
        char temp[20];
        sprintf(temp, "%s%s\r\n", host_msg, host);
        Rio_writen(clientfd, temp, strlen(temp));
    }
    if (!flagc){
        Rio_writen(clientfd, connection_msg, strlen(connection_msg));
    }
    if (!flagpc){
        Rio_writen(clientfd, proxyconnection_msg, strlen(proxyconnection_msg));
    }

    while((n = Rio_readlineb(&rio2, buf, MAXLINE)) != 0){
        Rio_writen(connfd, buf, n);
    }
    Close(clientfd);
}

void parse_uri(char *uri, char *host, char *portn, char *detailed){
    
    char *ptr;

    if (!(ptr = strstr(uri, "http://")))
        ptr = uri;
    else
        ptr += 7;

    int i = 0;
    while(*ptr != ':' && *ptr != '/'){
        host[i] = *ptr;
        ptr++;
        i++;
    }
    host[i] = 0;

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