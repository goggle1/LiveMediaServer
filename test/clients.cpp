
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <getopt.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>

#include "HTTPResponse.h"

#define MY_VERSION 	"1.0"
#define MAX_IP_LEN	16
#define MAX_ID_LEN	44
#define MAX_CODEC_LEN 4
#define MAX_BUF_LEN	4096

//stat: statistics.
typedef struct thread_stat_t
{
	int 	index;
	long	total_count_connection;
	long	total_count_request;
	long	total_count_response;
} THREAD_STAT_T;


typedef struct config_t
{
	char		server_ip[MAX_IP_LEN];	
	u_int16_t	server_port;
	char		live_id[MAX_ID_LEN];
	char		codec[MAX_CODEC_LEN];
	int			thread_num;
} CONFIG_T;

CONFIG_T g_config = 
{
	"192.168.160.202",
	5050,
	"0b49884b3b85f7ccddbe4e96e4ae2eae7a6dec56",
	"ts",
	10
};

int do_http_request(int client_fd, THREAD_STAT_T* statp)
{
	char send_buffer[MAX_BUF_LEN];
	snprintf(send_buffer, MAX_BUF_LEN-1, 
		"GET /livestream/%s.m3u8?codec=%s HTTP/1.1\r\n"
		"User-Agent: %s\r\n"
		"HOST: %s:%u\r\n"
		"Accept: */*\r\n"
		"\r\n", 
		g_config.live_id, g_config.codec, "TeslaBrowser", g_config.server_ip, g_config.server_port);
	send_buffer[MAX_BUF_LEN-1] = '\0';
	
	int total_len = strlen(send_buffer);
	int total_send_len = 0;
	while(1)
	{
		int send_len = send(client_fd, send_buffer+total_send_len, total_len-total_send_len, 0);
		//fprintf(stdout, "send %d, return %d\n", total_len-total_send_len, send_len);
		if(send_len == -1)
		{
			if(errno == EINTR || errno == EAGAIN)
			{
				continue;
			}
			else
			{				
				return -1;
			}
		}
		else
		{
			total_send_len += send_len;
			if(total_send_len >= total_len)
			{
				//fprintf(stdout, "http request success\n");
				statp->total_count_request ++;
				break;
			}
		}
	}

	char recv_buffer[MAX_BUF_LEN] = {0};
	int total_recv_len = 0;
	while(1)
	{
		int recv_len = recv(client_fd, recv_buffer+total_recv_len, sizeof(recv_buffer)-total_recv_len, 0);	
		//fprintf(stdout, "recv %d, return %d\n", sizeof(recv_buffer)-total_recv_len, recv_len);
		if(recv_len < 0)
		{
			if(errno == EINTR || errno == EAGAIN)
			{
				continue;
			}			
			return -1;
		}
		else if(recv_len == 0)
		{			
			return -1;
		}
		else
		{
			total_recv_len += recv_len;
		}
		
		char* temp = strstr(recv_buffer, "\r\n\r\n");		
		char* http_header_end = NULL;
		if(temp != NULL)
		{
			http_header_end = temp + strlen("\r\n\r\n");
			int http_header_len = http_header_end - recv_buffer;
			HTTPResponse http_response;
			http_response.parse(recv_buffer, http_header_len);
			char content_buffer[MAX_BUF_LEN] = {0};
			int total_content_len = 0;
			while(1)
			{
				int recv_len = recv(client_fd, content_buffer+total_content_len, sizeof(content_buffer)-total_content_len, 0);	
				//fprintf(stdout, "recv %d, return %d\n", sizeof(content_buffer)-total_content_len, recv_len);
				if(recv_len < 0)
				{
					if(errno == EINTR || errno == EAGAIN)
					{
						continue;
					}
					
					return -1;
				}
				else if(recv_len == 0)
				{					
					return -1;
				}
				else
				{
					total_content_len += recv_len;
					if(total_content_len >= http_response.fContentLength)
					{
						//fprintf(stdout, "http response success\n");
						//do_m3u8(content_buffer, http_response.fContentLength);
						statp->total_count_response ++;
						break;
					}
				}
			}
			if(total_content_len >= http_response.fContentLength)
			{
				break;
			}
		}
	}

	return 0;
}

int do_http_client(THREAD_STAT_T* statp)
{
	struct sockaddr_in client_addr; 
	client_addr.sin_family = AF_INET;
	client_addr.sin_port = htons(g_config.server_port);
	client_addr.sin_addr.s_addr = inet_addr(g_config.server_ip);
	
	int client_fd = socket(AF_INET, SOCK_STREAM, 0);
	if(client_fd < 0)
	{
		printf("socket() failure! errno=%d, %s\n", errno, strerror(errno));
		return -1; 
	}
	
	if(connect(client_fd, (struct sockaddr*)&client_addr, sizeof(client_addr)) < 0)
	{
		printf("connect() failure! errno=%d, %s\n", errno, strerror(errno));
		close(client_fd);
		client_fd = -1;
		return -1;
	}

	fprintf(stdout, "connection success!\n");
	statp->total_count_connection ++;

	while(1)
	{
		int ret = do_http_request(client_fd, statp);
		if(ret != 0)
		{
			break;
		}
	}	

	fprintf(stdout, "connection closed!\n");
	close(client_fd);
	client_fd = -1;
	
	return 0;
}


void* test_client(void* argp)
{
	THREAD_STAT_T* statp = (THREAD_STAT_T*)argp;
	
	while(1)
	{
		do_http_client(statp);
	}

	return NULL;
}


int stat_threads(THREAD_STAT_T* stats)
{
	return 0;
}

void print_usage(char* program_name)
{
	fprintf(stdout, "%s --help\n", program_name);
	fprintf(stdout, "%s -h\n", program_name);
	fprintf(stdout, "%s --version\n", program_name);
	fprintf(stdout, "%s -v\n", program_name);
	
	fprintf(stdout, "%s --server_ip=%s --server_port=%d --live_id=%s --codec=%s --thread_num=%d\n", 
		program_name, g_config.server_ip, g_config.server_port, g_config.live_id, g_config.codec, g_config.thread_num);
	fprintf(stdout, "%s -i %s -p %d -l %s -c %s -t %d\n", 
		program_name, g_config.server_ip, g_config.server_port, g_config.live_id, g_config.codec, g_config.thread_num);
}

int main(int argc, char* argv[])
{
	int ret = 0;
	
	// parse the command line.
    bool  have_unknown_opts = false;
    // command line
    // -i,   --server_ip
    // -p,  --server_port   
    // -l,   --live_id
    // -c,  --codec
    // -t, --thread_num
    // -v, --version
    // -h, --help
    // parse_cmd_line();
    static struct option orig_options[] = 
    {
    	{ "server_ip",	  1, 0, 'i' },
    	{ "server_port",  1, 0, 'p' },
    	{ "live_id",	  1, 0, 'l' },
    	{ "codec",  	  1, 0, 'c' },
        { "thread_num",   1, 0, 't' },
        { "version",      0, 0, 'v' },
        { "help",         0, 0, 'h' },        
        { NULL,           0, 0, 0   }
	};	
	while (true) 
	{
	    int c = -1;
	    int option_index = 0;
	  
	    c = getopt_long_only(argc, argv, "i:p:l:c:t:vh", orig_options, &option_index);
    	if (c == -1)
	        break;

	    switch (c) 
	    {
	    	case 'i':	
	            strncpy(g_config.server_ip, optarg, MAX_IP_LEN-1);
	            g_config.server_ip[MAX_IP_LEN-1] = '\0';
	            break;
	        case 'p':	
	            g_config.server_port = atoi(optarg);
	            break;
	        case 'l':	
	            strncpy(g_config.live_id, optarg, MAX_ID_LEN-1);
	            g_config.live_id[MAX_ID_LEN-1] = '\0';
	            break;
	        case 'c':	
	            strncpy(g_config.codec, optarg, MAX_CODEC_LEN-1);
	            g_config.codec[MAX_CODEC_LEN-1] = '\0';
	            break;
	        case 't':	
	            g_config.thread_num  = atoi(optarg);
	            break;	        
	        case 'h':
	            print_usage(argv[0]);	            	    
	            exit(0);
	            break;	          
	        case 'v':
	            fprintf(stdout, "%s: version %s\n", argv[0], MY_VERSION);
        	    exit(0);
        	    break;
	        case '?':
	        default:
	            have_unknown_opts = true;
	            break;
	    }
    }

	pthread_t* threads = (pthread_t*)malloc(g_config.thread_num*sizeof(pthread_t));
	if(threads == NULL)
	{
		fprintf(stderr, "%s: malloc for pthread_t failed\n", __FUNCTION__);
		return -1;
	}
	memset(threads, 0, g_config.thread_num*sizeof(pthread_t));

	THREAD_STAT_T* stats = (THREAD_STAT_T*)malloc(g_config.thread_num*sizeof(THREAD_STAT_T));
	if(stats == NULL)
	{
		fprintf(stderr, "%s: malloc for THREAD_STAT_T failed\n", __FUNCTION__);
		return -1;
	}
	memset(stats, 0, g_config.thread_num*sizeof(THREAD_STAT_T));
	
	int index = 0;
	for(index=0; index<g_config.thread_num; index++)
	{
		ret = pthread_create(&(threads[index]), NULL, test_client, (void*)(&stats[index]));
		if(ret < 0)
		{
			fprintf(stderr, "%s: pthread_create %d failed\n", __FUNCTION__, index);
			return -1;
		}
	}

	for(index=0; index<g_config.thread_num; index++)
	{
		pthread_join(threads[index], NULL);
	}

	ret = stat_threads(stats);
	if(ret == 0)
	{
		fprintf(stdout, "%s: test success!\n", __FUNCTION__);
	}
	else
	{
		fprintf(stdout, "%s: test failure!\n", __FUNCTION__);
	}

	return ret;
}

