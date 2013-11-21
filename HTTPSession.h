//file: HTTPSession.h
#ifndef __HTTPSESSION_H__
#define __HTTPSESSION_H__

#include "BaseServer/OSHeaders.h"
#include "BaseServer/StringFormatter.h"
#include "BaseServer/Task.h"
#include "BaseServer/TCPSocket.h"

#include "HTTPRequest.h"

#include "channel.h"
#include "HTTPClientSession.h"

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
        	QTSS_Error      SendData();
       	 	bool            IsFullRequest();
        	Bool16          Disconnect();
        	QTSS_Error      ProcessRequest();
        	QTSS_Error      ResponseGet();
        	Bool16 			ReadFileContent();
        	QTSS_Error		ResponseCmd();
        	QTSS_Error		ResponseCmdResult(char* cmd, char* result, char* reason);
        	QTSS_Error 		ResponseCmdListChannel();
        	QTSS_Error 		ResponseCmdAddChannel();
        	QTSS_Error 		ResponseCmdDelChannel();  
        	Bool16 			ResponseContent(char* content, int len, char* type);
        	QTSS_Error		ResponseFile(char* absolute_path);
        	QTSS_Error		ResponseError(HTTPStatusCode StatusCode);
	        void            MoveOnRequest();
	        QTSS_Error		ResponseLive();
	        QTSS_Error		ResponseLiveM3U8();
	        QTSS_Error		ResponseLiveSegment();
	        Bool16 			ReadSegmentContent();
	        QTSS_Error		ContinueLive();
	        QTSS_Error		ContinueLiveM3U8();
	        QTSS_Error		ContinueLiveSegment();

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
			// file
			int			fFd;
			char		fBuffer[kReadBufferSize];
			Bool16		fHaveRange;
			int64_t		fRangeStart;
			int64_t		fRangeStop;
			//livestream
			enum
			{
				kLiveM3U8 = 1,
				kLiveSegment = 2,
			};
			int			fLiveRequest;
			char		fLiveId[MAX_LIVE_ID];
			char		fLiveType[MAX_LIVE_TYPE];
			int			fLiveSeq;
			int			fLiveLen;
			HTTPClientSession*	fHttpClientSession;
			DATA_T* 	fData;
			int64_t		fDataPosition;
			
	        HTTPStatusCode fStatusCode;  	        


};

#endif


