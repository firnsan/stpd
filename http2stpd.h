#ifndef HTTP2STPD_H
#define HTTP2STPD_H

#include <pthread.h>
#include <map>

#include "utils.h"


class http2stpdConn
{
public:
	~http2stpdConn();
	void forwardRequest(reqarg_t *req);
	void backwardResponse();

	static http2stpdConn* getConnect(reqarg_t *req, char *proxyhost, int proxyport);
	static void* hanldeResponseThread(void* arg);
	
private:
	http2stpdConn(char* host, int port);
	
	//current max reqid in this connect form client-proxy to server-proxy
	unsigned int m_reqId; 
	int m_fd;
	
	//only one request can send to server blocking others
	pthread_mutex_t m_mutex;
	
	//reqs serialized by req_id in this connect ;
	typedef std::map<unsigned int, reqarg_t*>ReqMap;
	ReqMap m_reqs;

	typedef std::map<char*, http2stpdConn*> ConnMap;
	static ConnMap  m_connMap;
	
};

#endif