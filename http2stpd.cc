#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

#include "utils.h"
#include "http2stpd.h"

http2stpdConn::ConnMap http2stpdConn::m_connMap;

http2stpdConn* http2stpdConn::getConnect(reqarg_t *req, char *proxyhost, int proxyport)
{
	char host[MAX_FILE_NAME];
	char path[MAX_FILE_NAME];
	char prot[MAX_FILE_NAME];
	char hostport[MAX_FILE_NAME];
	int method, port;
	parserequest(req->reqline, &method, host, &port, path, prot);
	snprintf(hostport,sizeof(hostport), "%s%d", host, port);
	//保证clientproxy-->serverproxy的连接中只存在一个到该domain(origin sever)的
	if (m_connMap.find(hostport)==m_connMap.end()) {
		http2stpdConn *conn=new http2stpdConn(proxyhost, proxyport);
		m_connMap[hostport]=conn;
		return conn;
	} else {
		return m_connMap[hostport];
	}

}

http2stpdConn::http2stpdConn(char *host, int port)
	: m_fd(activesocket(host, port)),
	  m_reqId(0),
	  m_reqs()
{
	//新建一个线程，仲裁从serverproxy返回的response应该发往那个client
	pthread_t tid;
	pthread_create(&tid, NULL, &http2stpdConn::hanldeResponseThread, this);
	pthread_mutex_init(&m_mutex, NULL);
}

void http2stpdConn::forwardRequest(reqarg_t *req)
{	
	char reqhead1[MAX_HEADER];
	char host[MAX_FILE_NAME];
	char path[MAX_FILE_NAME];
	char prot[MAX_FILE_NAME];
	char reqid[MAX_FILE_NAME]; //储存请求序列号: "req=reqid\r\n"
	char tbuff[BSIZ];
	int server=m_fd;
	int i, portno, method;
	int n, rlen, tlen;
	reqarg_t *arg = (reqarg_t *)req;
	int client = arg->client;
	char *reqline = arg->reqline;
	char *reqhead = arg->reqhead;

	arg->req_id=++m_reqId; //该clientproxy-->serverproxy连接的reqid加1
	m_reqs[arg->req_id]=arg;

	parserequest(reqline, &method, host, &portno, path, prot);
	//sprintf(reqhead1, "%s %s HTTP/1.1\r\n", methods[method], path);//这是把绝对uri改成了相对的
	sprintf(reqhead1, "%s", arg->reqline);//不能把绝对url改成相对的，因为还要经过serverproxy的转发
	HTTPheaderremove_case(reqhead, "connection"); //xxx_case是忽略大小写
	//strcat(reqhead1, "Connection: close\r\n");//不能给server proxy发close）
	strcat(reqhead1, reqhead);

	while (1) {
		//将往clientproxy-->serverproxy连接发送数据的操作放在临界区中
		//避免共享该连接的client的http包杂揉在一起
		//因为一个完整http包发送分为好几步
		pthread_mutex_lock(&m_mutex);
		
		snprintf(reqid, sizeof(reqid), "req=%d\r\n", arg->req_id);
		n=strlen(reqid);
		if(write(server, reqid, n) < n) {
			pthread_mutex_unlock(&m_mutex);
			continue;
		}
		
		n = strlen(reqhead1);
		if (write(server, reqhead1, n) < n) {
			pthread_mutex_unlock(&m_mutex);
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
				pthread_mutex_unlock(&m_mutex);
				continue;
			}
		}
		pthread_mutex_unlock(&m_mutex);
		break;//正常情况是执行一次循环
	} //while(1)

}

void* http2stpdConn::hanldeResponseThread(void *arg)
{
	char reshead[MAX_HEADER];
	char resline[MAX_LINE];
	char tbuff[BSIZ];
	int status;
	int i, n, tlen, clen, chlen;
	http2stpdConn *conn=static_cast<http2stpdConn*>(arg);
	int server=conn->m_fd;
	unsigned int reqid;
	ReqMap *reqs;
	while(1) {
		reqs=&conn->m_reqs;
		reqid=readReqId(server); //首先读取reqid
		
		//从reqs表中找到对应该reqid的client
		if(reqs->find(reqid)==reqs->end())
			continue;
		reqarg_t *req=(*reqs)[reqid];
		int client=req->client;

		if (TCPreadline(server, resline, MAX_LINE) == 0) {
			continue;
		}
		if (HTTPreadheader(server, reshead, MAX_HEADER) == 0) {
			continue;
		}
		
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
					//close(server);
				}
			}
		}
		//已经处理完该reqid的请求，把该该请求删除
		reqs->erase(reqid);
		free(req);
	}
}

