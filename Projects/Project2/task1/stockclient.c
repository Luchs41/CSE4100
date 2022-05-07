/*
 * stockclient.c - An stock client
 */
/* $begin stckclientmain */
#include "csapp.h"

int main(int argc, char **argv) 
{
    int clientfd;
    char *host, *port, buf[MAXLINE];
    rio_t rio;

    if (argc != 3) {
		fprintf(stderr, "usage: %s <host> <port>\n", argv[0]);
		exit(0);
    }
    host = argv[1];
    port = argv[2];

    clientfd = Open_clientfd(host, port);
    Rio_readinitb(&rio, clientfd);

    while (Fgets(buf, MAXLINE, stdin) != NULL) {
		if(!strcmp(buf, "exit\n")) break;
		Rio_writen(clientfd, buf, strlen(buf));
		if(!strcmp(buf, "show\n")) {
			while(1) {
				Rio_readlineb(&rio, buf, MAXLINE);
				if(!strcmp(buf, "\n")) break;
				Fputs(buf, stdout);
			}
		}
		else {
			Rio_readlineb(&rio, buf, MAXLINE);
			Fputs(buf, stdout);
		}
    }
    Close(clientfd); //line:netp:echoclient:close
    exit(0);
}
/* $end stockclientmain */
