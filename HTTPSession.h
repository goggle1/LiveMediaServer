//file: HTTPSession.h
#ifndef __HTTPSESSION_H__
#define __HTTPSESSION_H__

#include "BaseServer/OSHeaders.h"
#include "BaseServer/StringFormatter.h"
#include "BaseServer/Task.h"
#include "BaseServer/TCPSocket.h"

#include "HTTPRequest.h"


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
        	Bool16 			ResponseCmdListSource();
        	Bool16 			ResponseCmdAddSource();
        	Bool16 			ResponseCmdDelSource();  
        	Bool16 			ResponseContent(char* content, int len, char* type);
        	Bool16 			ResponseFile(char* absolute_path);
        	Bool16 			ResponseFileNotFound(char* absolute_uri);
        	Bool16 			ResponseError(QTSS_RTSPStatusCode StatusCode);
	        void            MoveOnRequest();

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

			// from RTSP, 
	        QTSS_RTSPStatusCode fStatusCode;  	        


};

#endif


