#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>


#include "BaseServer/OS.h"
#include "BaseServer/Socket.h"
#include "BaseServer/SocketUtils.h"
#include "BaseServer/TimeoutTask.h"
#include "BaseServer/ev_epoll.h"

#include "public.h"
#include "HTTPListenerSocket.h"
#include "channel.h"
#include "HTTPClientSession.h"

#define DEFAULT_HTTP_SERVER_IP          0
#define DEFAULT_HTTP_SERVER_PORT        5050

#define MAX_URL_LEN		256

ChannelList g_channels;

int start_server()
{
	signal(SIGPIPE, SIG_IGN);
	
	OS::Initialize();
	OSThread::Initialize();

	Socket::Initialize();
	SocketUtils::Initialize(false);

#if !MACOSXEVENTQUEUE
#if MIO_SELECT
    ::select_startevents();//initialize the select() implementation of the event queue
#else
	::epoll_startevents();
#endif
#endif

	// here we create the TcpListener, or UdpListener, or RTSPListener
	/*
	Bool16 createListeners = true;
	if (qtssShuttingDownState == inInitialState) 
		createListeners = false;		
	sServer->Initialize(inPrefsSource, inMessagesSource, inPortOverride,createListeners);
	*/
	UInt32 server_ip    = DEFAULT_HTTP_SERVER_IP;
	UInt16 server_port  = DEFAULT_HTTP_SERVER_PORT;			
    HTTPListenerSocket* listenerp = new HTTPListenerSocket();
    if(listenerp == NULL)
    {
        fprintf(stderr, "%s %s[%d]: new HTTPListenerSocket failed!", 
        	__FILE__, __PRETTY_FUNCTION__, __LINE__);
        return -1;
    }
    OS_Error err = OS_NoErr;
    err = listenerp->Initialize(server_ip, server_port);
    if(err != OS_NoErr)
    {
        fprintf(stderr, "%s %s[%d]: HTTPListenerSocket Initialize failed!", 
        	__FILE__, __PRETTY_FUNCTION__, __LINE__);        
        return -1;
    }
    
	UInt32 numShortTaskThreads = 0;
    UInt32 numBlockingThreads = 0;
    UInt32 numThreads = 0;
    UInt32 numProcessors = 0;
    
    if (OS::ThreadSafe())
    {
        //numShortTaskThreads = sServer->GetPrefs()->GetNumThreads(); // whatever the prefs say
        if (numShortTaskThreads == 0) {
           numProcessors = OS::GetNumProcessors();
            // 1 worker thread per processor, up to 2 threads.
            // Note: Limiting the number of worker threads to 2 on a MacOS X system with > 2 cores
            //     results in better performance on those systems, as of MacOS X 10.5.  Future
            //     improvements should make this limit unnecessary.
            if (numProcessors > 2)
                numShortTaskThreads = 2;
            else
                numShortTaskThreads = numProcessors;
        }

        //numBlockingThreads = sServer->GetPrefs()->GetNumBlockingThreads(); // whatever the prefs say
        if (numBlockingThreads == 0)
            numBlockingThreads = 1;
            
    }
    if (numShortTaskThreads == 0)
        numShortTaskThreads = 1;

    if (numBlockingThreads == 0)
        numBlockingThreads = 1;

    numThreads = numShortTaskThreads + numBlockingThreads;
    //qtss_printf("Add threads shortask=%lu blocking=%lu\n",numShortTaskThreads, numBlockingThreads);
    TaskThreadPool::SetNumShortTaskThreads(numShortTaskThreads);
    TaskThreadPool::SetNumBlockingTaskThreads(numBlockingThreads);
    TaskThreadPool::AddThreads(numThreads);

    #if DEBUG
        qtss_printf("Number of task threads: %"_U32BITARG_"\n",numThreads);
    #endif
    
    // Start up the server's global tasks, and start listening
    TimeoutTask::Initialize();     // The TimeoutTask mechanism is task based,
                                // we therefore must do this after adding task threads
                                // this be done before starting the sockets and server tasks

	IdleTask::Initialize();
	Socket::StartThread();
	OSThread::Sleep(1000);

	//
	// On Win32, in order to call modwatch the Socket EventQueue thread must be
	// created first. Modules call modwatch from their initializer, and we don't
	// want to prevent them from doing that, so module initialization is separated
	// out from other initialization, and we start the Socket EventQueue thread first.
	// The server is still prevented from doing anything as of yet, because there
	// aren't any TaskThreads yet.
	/*
	 sServer->InitModules(inInitialState);
	 sServer->StartTasks();
	 sServer->SetupUDPSockets(); // udp sockets are set up after the rtcp task is instantiated
	 */
	listenerp->RequestEvent(EV_RE);  

	 
	// SWITCH TO RUN USER AND GROUP ID
	/*
	if (!sServer->SwitchPersonality())
		theServerState = qtssFatalErrorState;
	*/

	return 0;
}

int start_channel(CHANNEL_T* channelp)
{
	DEQUE_NODE* source_list = channelp->source_list;
	DEQUE_NODE* nodep = source_list;
	if(nodep == NULL)
	{
		return -1;
	}
	
	SOURCE_T* sourcep = (SOURCE_T*)nodep->datap;
	if(channelp->codec_ts)
	{		
		//StrPtrLen 	inURL("http://192.168.8.197:1180/1100000000000000000000000000000000000000.m3u8");
		//StrPtrLen 	inURL("http://lv.funshion.com/livestream/fd5f6b86b836e38c8eed27c9e66e3e6dcf0a69b2.m3u8?codec=ts");
		char* type = "ts";
		char url[MAX_URL_LEN];
		snprintf(url, MAX_URL_LEN-1, "/livestream/%s.m3u8?codec=%s", channelp->liveid, type);
		url[MAX_URL_LEN-1] = '\0';
		StrPtrLen inURL(url);
		HTTPClientSession* sessionp = new HTTPClientSession(sourcep->ip, sourcep->port, inURL, channelp->liveid, type);	
		if(sessionp == NULL)
		{
			return -1;
		}
	}
	if(channelp->codec_flv)
	{		
		char* type = "flv";
		char url[MAX_URL_LEN];
		snprintf(url, MAX_URL_LEN-1, "/livestream/%s.m3u8?codec=%s", channelp->liveid, type);
		url[MAX_URL_LEN-1] = '\0';
		StrPtrLen inURL(url);
		HTTPClientSession* sessionp = new HTTPClientSession(sourcep->ip, sourcep->port, inURL, channelp->liveid, type);	
		if(sessionp == NULL)
		{
			return -1;
		}
	}
	if(channelp->codec_mp4)
	{		
		char* type = "mp4";
		char url[MAX_URL_LEN];
		snprintf(url, MAX_URL_LEN-1, "/livestream/%s.m3u8?codec=%s", channelp->liveid, type);
		url[MAX_URL_LEN-1] = '\0';
		StrPtrLen inURL(url);
		HTTPClientSession* sessionp = new HTTPClientSession(sourcep->ip, sourcep->port, inURL, channelp->liveid, type);	
		if(sessionp == NULL)
		{
			return -1;
		}
	}	
	
	return 0;
}

int start_clients()
{
	DEQUE_NODE* channel_list = g_channels.GetChannels();
	DEQUE_NODE*	nodep = channel_list;
	while(nodep)
	{
		CHANNEL_T* channelp = (CHANNEL_T*)nodep->datap;
		start_channel(channelp);
		
		if(nodep->nextp == channel_list)
		{
			break;
		}
		nodep = nodep->nextp;
	}
	
	return 0;
}


int main(int argc, char* argv[])
{
	int ret = 0;
	
	char* config_file = ROOT_PATH"/channels.xml";
	ret = g_channels.ReadConfig(config_file);
	
	ret = start_server();
	if(ret != 0)
	{
		return ret;
	}

	ret = start_clients();
	if(ret != 0)
	{
		return ret;
	}	
	
	while(1)
	{
		sleep(1);
	}
	
	return 0;
}

