//file: HTTPListenerSocket.cpp

#include "HTTPSession.h"
#include "HTTPListenerSocket.h"

SESSION_T	g_http_sessions[MAX_SESSION_NUM] = {{0}};

Task*   HTTPListenerSocket::GetSessionTask(TCPSocket** outSocket)
{ 
	SESSION_T* sessionp = NULL;
	int index = 0;
	for(index=0; index<MAX_SEGMENT_NUM; index++)
	{
		if(g_http_sessions[index].sessionp == NULL)
		{
			sessionp = &g_http_sessions[index];
			memset(sessionp, 0, sizeof(SESSION_T));
			break;
		}
	}	
	if(sessionp == NULL)
	{
		return NULL;
	}
	
    HTTPSession* theTask = new HTTPSession(sessionp);
    
    *outSocket = theTask->GetSocket();  // out socket is not attached to a unix socket yet.
        
    return theTask;
}


Bool16 HTTPListenerSocket::OverMaxConnections(UInt32 buffer)
{
    return false;
}


