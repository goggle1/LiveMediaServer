
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
	int		Start();
	Bool16	IsDownloaded(SEGMENT_T* segp);
	void 	SetUrl(const StrPtrLen& inURL);
	int		SetSources(DEQUE_NODE* source_list);
	int 	Log(char* url, char* datap, UInt32 len);	
	int 	Write(StrPtrLen& file_name, char* datap, UInt32 len);
	int 	RewriteM3U8(M3U8Parser* parserp);	
	int 	MemoM3U8(M3U8Parser* parserp, time_t begin_time, time_t end_time);
	int 	MemoSegment(SEGMENT_T* segp, char* datap, UInt32 len, time_t begin_time, time_t end_time);
	MEMORY_T*	GetMemory() { return fMemory; }
	char*	GetSourceHost() { return fHost; }
	time_t	CalcBreakTime();

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
		int		SwitchSource();
		int		SetSource(SOURCE_T* sourcep);

		char				fHost[MAX_HOST_LEN];		
		//UInt32				fInAddr;
		//UInt16				fInPort;
		TCPClientSocket* 	fSocket;
		HTTPClient*			fClient;
		StrPtrLen   		fURL;
		StrPtrLen			fM3U8Path;
		CHANNEL_T*			fChannel;
		DEQUE_NODE*			fSourceList;
		DEQUE_NODE*			fSourceNow;
		MEMORY_T*			fMemory;
		char*				fType;
		
		UInt32          	fState;     // the state machine
		UInt32          	fDeathReason;

		TimeoutTask     	fTimeoutTask; // Kills this connection in the event the server isn't responding

		M3U8Parser		   	fM3U8Parser;  
		struct timeval		fM3U8BeginTime;
		struct timeval		fM3U8EndTime;
		struct timeval		fSegmentBeginTime;
		struct timeval		fSegmentEndTime;
		int					fGetIndex;
		int					fGetTryCount;
		
};

#endif
