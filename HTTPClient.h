
#ifndef __HTTPCLIENT_H__
#define __HTTPCLIENT_H__

#include "BaseServer/ClientSocket.h"

#define USER_AGENT	"LiveMediaServer"
#define MAX_HOST_LEN	64

class HTTPClient
{
	public:
		HTTPClient(ClientSocket* inSocket);
		~HTTPClient();				
		OS_Error    SendGetM3U8(char* url);
		OS_Error    SendGetSegment(char* url);
		//
        // Once you call all of the above functions, assuming they return an error, you
        // should call DoTransaction until it returns OS_NoErr, then you can move onto your
        // next request
        OS_Error    DoTransaction();
        OS_Error 	ReceiveResponse();
        
		UInt32      GetStatus()             { return fStatus; }
		Bool16      IsTransactionInProgress() { return fState != kInitial; }
		UInt32      GetContentLength()      { return fContentLength; }
        char*       GetContentBody()        { return fRecvContentBuffer; }
		
	protected:
		ClientSocket*	fSocket;	
		
		enum { kInitial, kRequestSending, kResponseReceiving, kHeaderReceived };
        UInt32      	fState;

        // Information we need to send the request
        //StrPtrLen   	fURL;
        char			fHost[MAX_HOST_LEN];

		enum
        {
	            kReqBufSize = 4095	            
        };
        char        fSendBuffer[kReqBufSize + 1];   // for sending requests
        char        fRecvHeaderBuffer[kReqBufSize + 1];// for receiving response headers
        char*       fRecvContentBuffer;             // for receiving response body
        // Tracking the state of our receives
        UInt32      fContentRecvLen;
        UInt32      fHeaderRecvLen;
        UInt32      fHeaderLen;

        // Response data we get back
        UInt32      fStatus;        
        UInt32      fContentLength;   
        Bool16		fChunked;
        Bool16		fChunkTail;
        
        //
        // For tracking media data that got read into the header buffer
        UInt32      fPacketDataInHeaderBufferLen;
        char*       fPacketDataInHeaderBuffer;
		
};

#endif
