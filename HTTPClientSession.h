
#ifndef __HTTPCLIENTSESSION_H__
#define __HTTPCLIENTSESSION_H__

#include "BaseServer/OSHeaders.h"
#include "BaseServer/Task.h"
#include "BaseServer/ClientSocket.h"
#include "BaseServer/TimeoutTask.h"

#include "HTTPClient.h"
#include "M3U8Parser.h"
#include "channel.h"

class HTTPClientSession : public Task
{
	public:
		HTTPClientSession(const StrPtrLen& inURL, CHANNEL_T* channelp, char* type);
virtual	~HTTPClientSession();
virtual     SInt64      Run();
	Bool16	IsDownloaded(SEGMENT_T* segp);
	void 	Set(const StrPtrLen& inURL);
	int 	Log(char* url, char* datap, UInt32 len);	
	int 	Write(StrPtrLen& file_name, char* datap, UInt32 len);
	int 	RewriteM3U8(M3U8Parser* parserp);	
	int 	MemoM3U8(M3U8Parser* parserp);
	int 	MemoSegment(SEGMENT_T* segp, char* datap, UInt32 len);
	MEMORY_T*	GetMemory() { return fMemory; }

		//
        // States. Find out what the object is currently doing
        enum
        {
            kSendingGetM3U8     = 0,
            kSendingGetSegment  = 1,
            kDone               = 2
        };
        //
        // Why did this session die?
        enum
        {
            kDiedNormally       = 0,    // Session went fine
            kTeardownFailed     = 1,    // Teardown failed, but session stats are all valid
            kRequestFailed      = 2,    // Session couldn't be setup because the server returned an error
            kBadSDP             = 3,    // Server sent back some bad SDP
            kSessionTimedout    = 4,    // Server not responding
            kConnectionFailed   = 5,    // Couldn't connect at all.
            kDiedWhilePlaying   = 6     // Connection was forceably closed while playing the movie
        };

	protected:
		//UInt32				fInAddr;
		//UInt16				fInPort;
		TCPClientSocket* 	fSocket;
		HTTPClient*			fClient;
		StrPtrLen   		fURL;
		StrPtrLen			fM3U8Path;
		CHANNEL_T*			fChannel;
		MEMORY_T*			fMemory;
		char*				fType;
		
		UInt32          	fState;     // the state machine
		UInt32          	fDeathReason;

		TimeoutTask     	fTimeoutTask; // Kills this connection in the event the server isn't responding

		M3U8Parser		   	fM3U8Parser;  
		struct timeval		fGetTime;
		int					fGetIndex;
		SEGMENT_T			fDownloadSegments[MAX_SEGMENT_NUM];
		int					fDownloadIndex;
		
};

#endif
