
// just for test.

#include <stdio.h>
#include <stdlib.h>
#include <string.h> 
#include <unistd.h> 
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define DEFAULT_PORT 5050

int main(int argc, char** argv)
{
    int cPort = DEFAULT_PORT;
    int cClient = 0;
    int cLen = 0;
    struct sockaddr_in cli;
    char cbuf[4096] = {0};  
    char* server_ip = NULL;
    
    if(argc < 2)
    {
        printf("Usage: client [server IP address]\n");
        return -1;
    }
    server_ip = argv[1];
    
    memset(cbuf, 0, sizeof(cbuf));
    
    cli.sin_family = AF_INET;
    cli.sin_port = htons(cPort);
    cli.sin_addr.s_addr = inet_addr(argv[1]);
    
    cClient = socket(AF_INET, SOCK_STREAM, 0);
    if(cClient < 0)
    {
        printf("socket() failure!\n");
        return -1; 
    }
    
    if(connect(cClient, (struct sockaddr*)&cli, sizeof(cli)) < 0)
    {
        printf("connect() failure!\n");
        return -1;
    }

	
    char send_buffer[4096];
    snprintf(send_buffer, 4095, 
    	"GET %s HTTP/1.1\r\n"
		"User-Agent: %s\r\n"
		"HOST: %s:%u\r\n"
		"Accept: */*\r\n"
		"\r\n", 
		"index.html", "TeslaBrowser", server_ip, DEFAULT_PORT);
	send_buffer[4095] = '\0';
	
	int total_len = strlen(send_buffer);
	int first_len = total_len/2;
	int second_len = total_len - first_len;
	int ret = send(cClient, send_buffer, first_len, 0);
	fprintf(stdout, "1st send %d, return %d\n", first_len, ret);
	ret = send(cClient, send_buffer+first_len, second_len, 0);
	fprintf(stdout, "2nd send %d, return %d\n", second_len, ret);
		
    cLen = recv(cClient, cbuf, sizeof(cbuf),0);    
    if((cLen < 0)||(cLen == 0))
    {
          printf("recv() failure!\n");
        return -1;
    }
    printf("recv() Data From Server: [%s]\n", cbuf);
    
    close(cClient);
    
    return 0;
}

/*
±àÒë´úÂë£ºgcc -o tcp_clt  client_tcp.c

Ö´ÐÐÃüÁî£º./tcp_clt 192.168.0.230
*/



