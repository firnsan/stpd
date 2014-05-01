/* Utility functions for the server to be written for Server Software. */

#ifndef NETWORK_H
#define NETWORK_H

#define MAX_HEADER     20480
#define MAX_LINE        2560
#define MAX_COMMAND    10240
#define MAX_FILE_NAME   2560
#define MAX_FILE_LEN   81920
#define GET 0
#define POST 1
#define BSIZ   8192

struct reqarg {
	unsigned int req_id;
	int client;
	char reqline[MAX_LINE];
	char reqhead[MAX_HEADER];
};

typedef reqarg reqarg_t;

extern char *methods[2];

extern int contentlength(char *header);

extern int parseresponse(char *statusline);
extern unsigned int readReqId(int fd);
extern void parserequest(char *request, int *method, char *host, int *portno, char *path, char *prot);



extern int passivesocket(int portnumber);
  /* Used by the server to open a passive socket attached to the port 
     numbered 'portnumber' on the server's machine, allowing a queue length 
     of 5 requests.  Returns the socket descriptor. */

extern int activesocket(char *hostname, int portnumber);
  /* Used by the client to connect to the port numbered 'portnumber' on the 
     server's machine, whose domain name is 'hostname', opening an active 
     socket for the connection.  Returns the socket descriptor.  */

extern int acceptconnection(int socket);
  /* Used by the server to accept a connection request from a client on a 
     passive socket, 'socket'.  When a request arrives, this function 
     allocates a new socket for the connection, and returns its descriptor.  */

extern int acceptfrom(int socket, char *client, int size);
  /* Same as 'acceptconnection' except that the IP address of the connecting
     client's machine is placed in the 'client' string.  No more than 'size'
     characters are copied into 'client'.  */

extern int TCPreadline(int socket, char *buffer, int size) ;
  /* Reads from a TCP socket 'socket' into a buffer 'buffer' of size 'size'.  
     All characters up to and including the first end of line ('\n') character 
     will be read, up to a maximum of size-1 bytes.  The string read will 
     be terminated with a null character.  The function returns the length 
     of the string read.  */

extern int HTTPreadheader (int socket, char *buffer, int size);
  /* Reads an HTTP message header from a TCP socket into a buffer 'buffer'
     of size 'size'.  All characters up to and including the first
     "\r\n\r\n" sequence will be read, up to a maximum of size-1 bytes.
     The string read will be terminated with a null character.  The
     function returns the length of the string read.  */

extern int HTTPheadervalue(char *header, char *key, char *value);
  /* Searches an HTTP message header (read by 'HTTPreadheader') for a line
     with field name 'key'.  If found, the rest of the matching line (after
     the ": ") is returned in 'value' as a null-terminated string, and the
     function returns 1; otherwise the function returns 0.  */

extern void HTTPheaderremove(char *header, char *key);
  /* Searches an HTTP message header (read by 'HTTPreadheader') for a line
     with field name 'key'.  If found, the whole line is deleted from the 
     header.  */

	extern int HTTPheadervalue_case(char *header, char *key, char *value) ;
  /* Searches an HTTP message header (read by 'HTTPreadheader') for a line
     with field name 'key'.  If found, the rest of the matching line (after
     the ": ") is returned in 'value' as a null-terminated string, and the
     function returns 1; otherwise the function returns 0.  */

extern void HTTPheaderremove_case (char *header, char *key);
  /* Searches an HTTP message header (read by 'HTTPreadheader') for a line
     with field name 'key'.  If found, the whole line is deleted from the 
     header.  */

#endif
