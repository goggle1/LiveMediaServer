//file: HTTPListenerSocket.cpp

#include "HTTPSession.h"
#include "HTTPListenerSocket.h"

SESSION_T	g_http_sessions[MAX_SESSION_NUM] = {{0}};
int			g_http_session_pos = 0;

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


