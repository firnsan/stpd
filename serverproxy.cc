#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>

#include "utils.h"


void *request(void *a)
{
	char reqhead1[MAX_HEADER];
	char reshead[MAX_HEADER];
	char resline[MAX_LINE];
	char host[MAX_FILE_NAME];
	char path[MAX_FILE_NAME];
	char prot[MAX_FILE_NAME];
	char reqid[MAX_FILE_NAME]; //储存序列号reqid:"req=reqid\r\n"
	char tbuff[BSIZ];
	int server, i, portno, method, status;
	int n, rlen, tlen, clen;
	unsigned int chlen;
	reqarg_t *arg = (reqarg_t *)a;
	int client = arg->client;
	char *reqline = arg->reqline;
	char *reqhead = arg->reqhead;

	parserequest(reqline, &method, host, &portno, path, prot);
	sprintf(reqhead1, "%s %s HTTP/1.1\r\n", methods[method], path);
	HTTPheaderremove_case(reqhead, "connection"); //xxx_case是忽略大小写
	strcat(reqhead1, "Connection: close\r\n");
	strcat(reqhead1, reqhead);
	while (1) {
		if ((server = activesocket(host, portno)) < 0) {
			sleep(1);
			continue;
		}
		n = strlen(reqhead1);
		if (write(server, reqhead1, n) < n) {
			close(server);
			continue;
		}
		if (method == POST) {
			rlen = contentlength(reqhead);
			i = 0;
			n = 0;
			while (rlen > 0) {
				tlen = BSIZ;
				if (rlen < tlen) {
					tlen = rlen;
				}
				n = read(client, tbuff, tlen);
				if (n <= 0) break;
				i = write(server, tbuff, n);
				if (i < n) break;
				rlen -= n;
			}
			if (n <= 0 || i < n) {
				close(server);
				continue;
			}
		}
		if (TCPreadline(server, resline, MAX_LINE) == 0) {
			close(server);
			continue;
		}
		if (HTTPreadheader(server, reshead, MAX_HEADER) == 0) {
			close(server);
			continue;
		}
		
		break;//正常情况是执行一次循环
	}//while(1)

	//返回的response头部加上req=xxx\r\n
	snprintf(reqid, sizeof(reqid), "req=%d\r\n", arg->req_id);
	n=strlen(reqid);
	write(client, reqid, n);
		
	write(client, resline, strlen(resline));
	HTTPheaderremove_case(reshead, "connection");
	write(client, reshead, strlen(reshead));
	if (HTTPheadervalue_case(reshead, "Transfer-Encoding", resline) && strcasecmp(resline, "chunked") == 0) {
		while (1) {
			TCPreadline(server, resline, MAX_LINE);
			write(client, resline, strlen(resline));
			sscanf(resline, "%x", &chlen);
			if (chlen == 0) break;
			while (chlen > 0) {
				tlen = BSIZ;
				if (chlen < tlen) {
					tlen = chlen;
				}
				n = read(server, tbuff, tlen);
				chlen -= n;
				i = write(client, tbuff, n);
			}
			read(server, tbuff, 1);
			if (*tbuff != '\r') printf("Error %d\n", *tbuff);
			read(server, tbuff, 1);
			if (*tbuff != '\n') printf("Error %d\n", *tbuff);
			write(client, "\r\n", 2);
		}
		read(server, tbuff, 1);
		if (*tbuff != '\r') printf("Error %d\n", *tbuff);
		read(server, tbuff, 1);
		if (*tbuff != '\n') printf("Error %d\n", *tbuff);
		write(client, "\r\n", 2);
	}
	else {
		status = parseresponse(resline);
		if (status != 204 && status != 304) {
			clen = contentlength(reshead);
			while (clen > 0) {
				tlen = BSIZ;
				if (clen < tlen) {
					tlen = clen;
				}
				n = read(server, tbuff, tlen);
				if (n <= 0) break;
				i = write(client, tbuff, n);
				if (i < n) break;
				clen -= n;
			}
			if (n <= 0 || i < n) {
				close(server);
			}
		}
	}
	return NULL;
}

void *connection(void *a)
{
	char temp[MAX_FILE_NAME];
	reqarg_t *arg;
	int client = *(int *)a;

	do {
		arg = (reqarg_t *) malloc(sizeof(reqarg_t));
		arg->client = client;
		//获取该请求的序列号reqid
		if( (arg->req_id=readReqId(client)) < 0) {
			break;
		}
			
		TCPreadline(client, arg->reqline, MAX_LINE);
		HTTPreadheader(client, arg->reqhead, MAX_HEADER);
		request(arg);//每个请求都和服务器建立一个连接
		
	} while (!HTTPheadervalue_case(arg->reqhead, "connection", temp) || strcasecmp(temp, "close") != 0);
	//当"connection"是close时，断开与clientproxy的连接.
	//实际依据题意，clientproxy不会发送‘connection:close’，以保持clientproxy-->serverproxy连接时间尽量长
	
	close(client);
	return NULL;
}

int main(int argc, char *argv[])
{
	int port, serversocket;
	int *arg;
	pthread_t t;

	sigignore(SIGPIPE);
	port = atoi(argv[1]);
	printf("**port %d\n", port);
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
