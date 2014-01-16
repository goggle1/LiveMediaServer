
#ifndef __HTTPCLIENT_H__
#define __HTTPCLIENT_H__

#include "BaseServer/ClientSocket.h"

#include "public.h"
#include "channel.h"

class HTTPClient
{
	public:
		HTTPClient(TCPClientSocket* inSocket/*, CHANNEL_T* channelp*/);
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

        //int			SetSources(DEQUE_NODE* source_list);
        int			Disconnect();
        int			SetSource(u_int32_t ip, u_int16_t port);

		// Information we need to send the request
        char			fHost[MAX_HOST_LEN];
        char*			fUrl;
        
		struct timeval		fBeginTime;
        struct timeval		fEndTime;
        
	protected:
		TCPClientSocket*	fSocket;	
		//CHANNEL_T*			fChannel;
		//DEQUE_NODE*			fSource;
		u_int32_t			fSourceIp;
		u_int16_t			fSourcePort;
		
		enum { kInitial, kRequestSending, kResponseReceiving, kHeaderReceived };
        UInt32      		fState;   
        
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
