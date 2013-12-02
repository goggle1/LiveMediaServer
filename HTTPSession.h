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

#define MAX_CMD_LEN	64
typedef struct cmd_t
{
	char	cmd[MAX_CMD_LEN];
	int		format;	// 0. plain, 1. html
} CMD_T;

#define 	MAX_SESSION_NUM	(1024*1024)
#define SESSION_CMD		(0x01<<0)
#define SESSION_LIVE	(0x01<<1)
#define SESSION_FILE	(0x01<<2)
//session statistics
typedef struct session_t
{	
	void*			sessionp;
	struct timeval	begin_time;	
	struct timeval	end_time;
	u_int64_t		upload_bytes;
	u_int64_t		download_bytes;
	u_int32_t		session_type;
	u_int32_t		remote_ip;
	u_int16_t		remote_port;
} SESSION_T;

extern SESSION_T 	g_http_sessions[MAX_SESSION_NUM];
extern int	g_http_session_num;

class HTTPSession : public Task
{
public:
            HTTPSession(SESSION_T* statp);
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
        	Bool16 			ReadSegmentContent();
        	Bool16 			ReadCmdContent();
        	QTSS_Error		ResponseCmd();
        	QTSS_Error		ResponseCmdResult(char* cmd, char* return_val, char* result, char* reason);
        	QTSS_Error 		ResponseCmdListChannel();
        	QTSS_Error 		ResponseCmdAddChannel();
        	QTSS_Error 		ResponseCmdDelChannel(); 
        	QTSS_Error 		ResponseCmdUpdateChannel(CHANNEL_T* findp, CHANNEL_T* channelp);
        	QTSS_Error 		ResponseCmdChannelStatus();
        	QTSS_Error 		ResponseCmdSessionStatus();
        	Bool16 			ResponseContent(char* content, int len, char* type);
        	Bool16 			ResponseHeader(char* content, int len, char* type);
        	QTSS_Error		ResponseFile(char* absolute_path);
        	QTSS_Error		ResponseError(HTTPStatusCode StatusCode);
	        void            MoveOnRequest();
	        QTSS_Error		ResponseLive();
	        QTSS_Error		ResponseLiveM3U8();
	        QTSS_Error		ResponseLiveSegment();	        
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
			// cmd for macross
			CMD_T		fCmd;
			char*		fCmdBuffer;
			int64_t		fCmdBufferSize;
			int64_t		fCmdContentLength;
			int64_t		fCmdContentPosition;
			
	        HTTPStatusCode fStatusCode; 

	        //session statistics
			SESSION_T*	fSessionp;
};

#endif


