//file: HTTPListenerSocket.cpp

#include "config.h"
#include "HTTPSession.h"
#include "HTTPListenerSocket.h"

u_int64_t		g_download_bytes = 0;
SESSION_T		g_http_sessions[MAX_SESSION_NUM] = {{0}};
int				g_http_session_pos = 0;
#if 0
FILE*			g_log = NULL;
#endif

HTTPListenerSocket::HTTPListenerSocket()
{
#if 0
	char FileName[PATH_MAX] = {'\0'};
	snprintf(FileName, PATH_MAX, "%s/sessions_%ld_%06ld.log", 
		g_config.work_path, g_start_time.tv_sec, g_start_time.tv_usec);
	FileName[PATH_MAX-1] = '\0';
	g_log = fopen(FileName, "a");
	fprintf(stdout, "%s: open log %s return 0x%016lX\n", __PRETTY_FUNCTION__, FileName, (u_int64_t)g_log);
#endif
}

HTTPListenerSocket::~HTTPListenerSocket()
{
#if 0
	if(g_log != NULL)
	{
		fclose(g_log);
		g_log = NULL;
	}
#endif
}

Task*   HTTPListenerSocket::GetSessionTask(TCPSocket** outSocket, struct sockaddr_in* addr)
{ 
	SESSION_T* sessionp = NULL;
	int index = 0;
	for(index=0; index<MAX_SESSION_NUM; index++)
	{
		if(g_http_sessions[index].sessionp == NULL)
		{
			if(index > g_http_session_pos)
			{
				g_http_session_pos = index;
			}
			sessionp = &g_http_sessions[index];
			memset(sessionp, 0, sizeof(SESSION_T));
			break;
		}
	}	
	if(sessionp == NULL)
	{
		return NULL;
	}
	
	sessionp->remote_ip = addr->sin_addr.s_addr;
	sessionp->remote_port = addr->sin_port;
	
    HTTPSession* theTask = new HTTPSession(sessionp);
    
    *outSocket = theTask->GetSocket();  // out socket is not attached to a unix socket yet.
        
    return theTask;
}


Bool16 HTTPListenerSocket::OverMaxConnections(UInt32 buffer)
{
    return false;
}


