#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include "utils.h"
#include "http2stpd.h"


char proxyhost[MAX_FILE_NAME]; //serverproxy hostname
int proxyport; //serverproxy port

void *connection(void *a)
{
	char temp[MAX_FILE_NAME];
	reqarg_t *arg;
	int client = *(int *)a;

	do {
		arg = (reqarg_t *) malloc(sizeof(reqarg_t));
		arg->client = client;
		if (TCPreadline(client, arg->reqline, MAX_LINE) == 0) {
			break;
		}
		HTTPreadheader(client, arg->reqhead, MAX_HEADER);

		http2stpdConn *conn=http2stpdConn::getConnect(arg, proxyhost, proxyport);
		conn->forwardRequest(arg);
	} while (!HTTPheadervalue_case(arg->reqhead, "connection", temp) || strcasecmp(temp, "close") != 0);//当"connection"是close时，断开与client的连接
	
	close(client);
	return NULL;
}

int main(int argc, char *argv[])
{
	if (argc!=3) {
		printf("Usage: %s port serverproxyhost:port\n", argv[0]);
		exit(-1);
	}
	int port, serversocket;
	int *arg;
	pthread_t t;
	char temp[MAX_FILE_NAME];

	sigignore(SIGPIPE);
	port = atoi(argv[1]);
	sscanf(argv[2], "%[^:]:%[^ ]", proxyhost, temp);
	proxyport=atoi(temp);
	
	printf("**local port %d\n", port);
	printf("**remote port %s:%d\n", proxyhost, proxyport);
	
	if ((serversocket = passivesocket(port)) < 0) {
		perror("open");
		exit(1);
	}
	while(1) {
		arg = (int *)malloc(sizeof(int));
		*arg = acceptconnection(serversocket);
		pthread_create(&t, NULL, connection, arg);
	}
	return 0;
}
