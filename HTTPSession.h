//file: HTTPSession.h
#ifndef __HTTPSESSION_H__
#define __HTTPSESSION_H__

#include "BaseServer/OSHeaders.h"
#include "BaseServer/StringFormatter.h"
#include "BaseServer/Task.h"
#include "BaseServer/TCPSocket.h"

#include "HTTPRequest.h"

#include "channel.h"

class HTTPSession : public Task
{
public:
            HTTPSession();
virtual    ~HTTPSession();
            TCPSocket*  GetSocket();
            //inherited from Task
virtual     SInt64      Run();

protected:
			QTSS_Error      RecvData();
        	Bool16          SendData();
       	 	bool            IsFullRequest();
        	Bool16          Disconnect();
        	QTSS_Error      ProcessRequest();
        	Bool16          ResponseGet();
        	Bool16 			ReadFileContent();
        	Bool16 			ResponseCmd();
        	Bool16 			ResponseCmdResult(char* cmd, char* result, char* reason);
        	Bool16 			ResponseCmdListChannel();
        	Bool16 			ResponseCmdAddChannel();
        	Bool16 			ResponseCmdDelChannel();  
#if 0
        	Bool16 			ResponseCmdListSource();
        	Bool16 			ResponseCmdAddSource();
        	Bool16 			ResponseCmdDelSource();  
#endif
        	Bool16 			ResponseContent(char* content, int len, char* type);
        	Bool16 			ResponseFile(char* absolute_path);
        	Bool16 			ResponseError(HTTPStatusCode StatusCode);
	        void            MoveOnRequest();
	        Bool16 			ResponseLive();
	        Bool16			ResponseLiveM3U8();
	        Bool16			ResponseLiveSegment();
	        Bool16 			ReadSegmentContent();

			TCPSocket	fSocket;
		
			//CONSTANTS:
			enum
			{
				kRequestBufferSizeInBytes = 2048,		 //UInt32			 
			};
			char		fRequestBuffer[kRequestBufferSizeInBytes];
			//received all the data, maybe one and half rtsp request.
			StrPtrLen	fStrReceived;
			//one full rtsp request.
			StrPtrLen	fStrRequest; 
			//
			HTTPRequest fRequest;
	
			//CONSTANTS:
			enum
			{
				kResponseBufferSizeInBytes = 2048*10,		 //UInt32	
				kReadBufferSize = 1024*16,
			};
			char		fResponseBuffer[kResponseBufferSizeInBytes];
			//one full rtsp response.
			StrPtrLen	fStrResponse; 
			//rtsp response left, which will be sended again.
			StrPtrLen	fStrRemained;
			// 
			StringFormatter 	fResponse;
			// 
			int			fFd;
			char		fBuffer[kReadBufferSize];
			//
			DATA_T* 	fMemory;
			int64_t		fMemoryPosition;

			// from RTSP, 
	        QTSS_RTSPStatusCode fStatusCode;  	        


};

#endif


