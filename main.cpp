#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <getopt.h>

#include "BaseServer/OS.h"
#include "BaseServer/Socket.h"
#include "BaseServer/SocketUtils.h"
#include "BaseServer/TimeoutTask.h"
#include "BaseServer/ev_epoll.h"

#include "public.h"
#include "config.h"
#include "HTTPListenerSocket.h"
#include "channel.h"
#include "HTTPClientSession.h"

#define DEFAULT_CONFIG_FILE          	"./LiveMediaServer.xml"
#define DEFAULT_HTTP_SERVER_IP          0
#define DEFAULT_HTTP_SERVER_PORT        5050

#define MAX_URL_LEN		256

char* 		g_config_file = DEFAULT_CONFIG_FILE;
CONFIG_T	g_config = {};
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
	UInt32 server_ip    = inet_network(g_config.ip);
	UInt16 server_port  = g_config.port;			
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
        #if 0
            if (numProcessors > 2)
                numShortTaskThreads = 2;
            else
                numShortTaskThreads = numProcessors;
        #else
        	numShortTaskThreads = numProcessors;
        #endif
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

void print_usage(char* program_name)
{
	fprintf(stdout, "%s --help\n", program_name);
	fprintf(stdout, "%s -h\n", program_name);
	fprintf(stdout, "%s --version\n", program_name);
	fprintf(stdout, "%s -v\n", program_name);
	
	fprintf(stdout, "%s --config_file=%s\n", 
		program_name, DEFAULT_CONFIG_FILE);
	fprintf(stdout, "%s -c %s\n", 
		program_name, DEFAULT_CONFIG_FILE);
}

int parse_cmd_line(int argc, char* argv[])
{		
	bool  have_unknown_opts = false;
	// command line
	// -c, --config_file
	// -v, --version
	// -h, --help
	// parse_cmd_line();
	static struct option orig_options[] = 
	{
		{ "config_file",  1, 0, 'c' },
		{ "version",	  0, 0, 'v' },
		{ "help",		  0, 0, 'h' },		  
		{ NULL, 		  0, 0, 0	}
	};	
	while (true) 
	{
		int c = -1;
		int option_index = 0;
	  
		c = getopt_long_only(argc, argv, "c:vh", orig_options, &option_index);
		if (c == -1)
			break;

		switch (c) 
		{			
			case 'c':	
				g_config_file = strdup(optarg);
				break;					
			case 'h':
				print_usage(argv[0]);						
				exit(0);
				break;			  
			case 'v':
				fprintf(stdout, "%s: version %s\n", argv[0], MY_VERSION);
				exit(0);
				break;
			case '?':
			default:
				have_unknown_opts = true;
				break;
		}
	}

	if(have_unknown_opts)
	{
		return -1;
	}

	return 0;
}

int main(int argc, char* argv[])
{
	int ret = 0;

	ret = parse_cmd_line(argc, argv);
	if(ret != 0)
	{
		fprintf(stderr, "parse_cmd_line error!\n");
		return ret;
	}

	ret = config_read(&g_config, g_config_file);
	if(ret != 0)
	{
		fprintf(stderr, "config_read %s error!\n", g_config_file);
		return ret;
	}
	
	char* channels_file = g_config.channels_file;
	ret = g_channels.ReadConfig(channels_file);
	if(ret != 0)
	{
		fprintf(stderr, "g_channels.ReadConfig %s error!\n", channels_file);
		//return ret;
	}
	
	ret = start_server();
	if(ret != 0)
	{
		fprintf(stderr, "start_server error!\n");
		return ret;
	}

	ret = start_clients();
	if(ret != 0)
	{
		fprintf(stderr, "start_clients error!\n");
		return ret;
	}	

	int count = 0;
	while(1)
	{
		count ++;
		#if 0
		fprintf(stdout, "%s: count=%d\n", __PRETTY_FUNCTION__, count);
		if(count > 60*5)
		{			
			break;
		}
		#endif
		
		sleep(1);
	}
	
	return 0;
}

