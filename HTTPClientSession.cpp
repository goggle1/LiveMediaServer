
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
       

#include "HTTPClientSession.h"

HTTPClientSession::HTTPClientSession()
	:fTimeoutTask(this, 60)
{
	//StrPtrLen 	inURL("http://192.168.8.197:1180/1100000000000000000000000000000000000000.m3u8");
	StrPtrLen 	inURL("/1100000000000000000000000000000000000000.m3u8");
	//StrPtrLen 	inURL("http://lv.funshion.com/livestream/fd5f6b86b836e38c8eed27c9e66e3e6dcf0a69b2.m3u8?codec=ts");
	//StrPtrLen 	inURL("/livestream/fd5f6b86b836e38c8eed27c9e66e3e6dcf0a69b2.m3u8?codec=ts");
	UInt32		inHTTPCookie = 0;
	UInt32		inAddr = 0;
	UInt16		inPort = 0;	
	//todo: param set value
	char* 	source_ip = "192.168.8.197";
	//char* 	source_ip = "220.181.167.45";
	inPort = 1180;
	//inPort	= 80;
	inAddr = inet_network(source_ip);
	
	//fSocket = new HTTPClientSocket(inURL, inHTTPCookie, Socket::kNonBlockingSocketType);
	fSocket = new TCPClientSocket(Socket::kNonBlockingSocketType);
	fSocket->Set(inAddr, inPort);
	
	fClient = new HTTPClient(fSocket);
	fClient->Set(inURL);
	
	this->Signal(Task::kStartEvent);

	fprintf(stdout, "%s\n", __PRETTY_FUNCTION__);
}

HTTPClientSession::~HTTPClientSession()
{
	fprintf(stdout, "%s\n", __PRETTY_FUNCTION__);
}

SInt64 HTTPClientSession::Run()
{
	Task::EventFlags theEvents = this->GetEvents(); 
	
	if (theEvents & Task::kStartEvent)
    {
    	// todo:
    }

    if (theEvents & Task::kTimeoutEvent)
    {
		if(fState == kDone)
			return 0;
			
        fDeathReason = kSessionTimedout;
        fState = kDone;
        return 0;
    }

    // We have been told to delete ourselves. Do so... NOW!!!!!!!!!!!!!!!
    if (theEvents & Task::kKillEvent)
    {
        return -1;
    }

    // Refresh the timeout. There is some legit activity going on...
    fTimeoutTask.RefreshTimeout();

    OS_Error theErr = OS_NoErr;    
    while ((theErr == OS_NoErr) && (fState != kDone))
    {
        //
        // Do the appropriate thing depending on our current state
        switch (fState)
        {
            case kSendingGetM3U8:
            {
            	theErr = fClient->SendGetM3U8();
            	if (theErr == OS_NoErr)
                {   
                    if (fClient->GetStatus() != 200)
                    {
                        theErr = ENOTCONN; // Exit the state machine
                        break;
                    }
                    else
                    {
                        fM3U8Parser.Parse(fClient->GetContentBody(), fClient->GetContentLength());
                        fGetSegmentNum = 0;
                        fState = kSendingGetSegment;
                    }
                }
                
                break;
            }
            case kSendingGetSegment:
            {
            	char* url = fM3U8Parser.fSegments[fGetSegmentNum].url;
            	
            	theErr = fClient->SendGetSegment(url);
            	if (theErr == OS_NoErr)
                {   
                	if (fClient->GetStatus() == 404)
                    {
                        fGetSegmentNum ++;
                        // if all the segments downloaded, get m3u8 again
                        if(fGetSegmentNum >= 3)
                        {
                        	fState = kSendingGetM3U8;
                        }
                    }
                    else if (fClient->GetStatus() != 200)
                    {
                        theErr = ENOTCONN; // Exit the state machine
                        break;
                    }
                    else
                    {
                    	fGetSegmentNum ++;
                        // if all the segments downloaded, get m3u8 again
                        if(fGetSegmentNum >= 3)
                        {
                        	fState = kSendingGetM3U8;
                        }
                    }
                }
                
                break;
            }
        }
    }

    if ((theErr == EINPROGRESS) || (theErr == EAGAIN))
    {
        //
        // Request an async event
        fSocket->GetSocket()->SetTask(this);
        fSocket->GetSocket()->RequestEvent(fSocket->GetEventMask());
    }
    else if (theErr != OS_NoErr)
    {
        //
        // We encountered some fatal error with the socket. Record this as a connection failure
        if (fClient->GetStatus() != 200)
            fDeathReason = kRequestFailed;
        else
            fDeathReason = kConnectionFailed;

        fState = kDone;
    }
	
	return 0;
}




