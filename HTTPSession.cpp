//file: HTTPSession.cpp

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
       
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "BaseServer/StringParser.h"

#include "public.h"
#include "HTTPSession.h"
#include "channel.h"

#define BASE_SERVER_NAME 	"TeslaStreamingServer"
#define BASE_SERVER_VERSION "1.0"

#define CONTENT_TYPE_TEXT_XML	"text/xml"
#define CONTENT_TYPE_TEXT_HTML	"text/html"
#define CONTENT_TYPE_VIDEO_MP4	"video/mp4"

#define CHARSET_UTF8		"utf-8"

#define URI_CMD  			"/cmd/"
#define CMD_LIST_CHANNEL  	"/cmd/list_channel/"
#define CMD_ADD_CHANNEL  	"/cmd/add_channel/"
#define CMD_DEL_CHANNEL  	"/cmd/del_channel/"
#define CMD_LIST_SOURCE  	"/cmd/list_source/"
#define CMD_ADD_SOURCE  	"/cmd/add_source/"
#define CMD_DEL_SOURCE  	"/cmd/del_source/"

#define MAX_REASON_LEN		256

static char template_response_http_error[] = 
            "HTTP/1.0 %s %s\r\n" //error_code error_reason
            "Server: %s/%s\r\n" //ServerName ServerVersion
            "\r\n";

Bool16 file_exist(char* abs_path)
{
	int ret = 0;
	ret = access(abs_path, R_OK);
	if(ret == 0)
	{
		return true;
	}

	
	return false;
}

char* file_suffix(char* abs_path)
{
	char* suffix = rindex(abs_path, '.');
	return suffix;
}

char* content_type_by_suffix(char* suffix)
{
	if(suffix == NULL)
	{
		return CONTENT_TYPE_TEXT_HTML;
	}
	
	if(strcasecmp(suffix, ".htm") == 0)
	{
		return CONTENT_TYPE_TEXT_HTML;
	}
	else if(strcasecmp(suffix, ".html") == 0)
	{
		return CONTENT_TYPE_TEXT_HTML;
	}
	else if(strcasecmp(suffix, ".xml") == 0)
	{
		return CONTENT_TYPE_TEXT_XML;
	}
	else if(strcasecmp(suffix, ".mp4") == 0)
	{
		return CONTENT_TYPE_VIDEO_MP4;
	}
	else
	{
		return CONTENT_TYPE_TEXT_HTML;
	}
}

HTTPSession::HTTPSession():
    fSocket(NULL, Socket::kNonBlockingSocketType),
    fStrReceived((char*)fRequestBuffer, 0),
    fStrRequest(fStrReceived),
    fStrResponse((char*)fResponseBuffer, 0),
    fStrRemained(fStrResponse),
    fResponse(NULL, 0)    
{
	fprintf(stdout, "%s %s[%d][0x%016lX] \n", __FILE__, __PRETTY_FUNCTION__, __LINE__, (long)this);
	fFd	= -1;
    fStatusCode     = 0;
}

HTTPSession::~HTTPSession()
{
	if(fFd != -1)
	{
		close(fFd);
		fFd = -1;
	}
    fprintf(stdout, "%s %s[%d][0x%016lX] \n", __FILE__, __PRETTY_FUNCTION__, __LINE__, (long)this);
}

TCPSocket* HTTPSession::GetSocket() 
{ 
    return &fSocket;
}

SInt64     HTTPSession::Run()
{ 
    Task::EventFlags events = this->GetEvents();    
    if(events == 0x00000000)
    {
    	// epoll_wait EPOLLERR or EPOLLHUP
    	// man epoll_ctl
    	fprintf(stdout, "%s %s[%d][0x%016lX] events=0x%08X\n",
			__FILE__, __PRETTY_FUNCTION__, __LINE__, (long)this, events);
		Disconnect();
    	return -1;
    }
    
    if(events & Task::kKillEvent)
    {
        fprintf(stdout, "%s %s[%d][0x%016lX]: get kKillEvent[0x%08X] \n", 
            __FILE__, __PRETTY_FUNCTION__, __LINE__, (long)this, events); 
        return -1;
    } 

	int willRequestEvent = 0;
	
    if(events & Task::kWriteEvent)
    {
    	Bool16 sendDone = SendData();    
    	if(!sendDone)
    	{
    		willRequestEvent = willRequestEvent | EV_WR;
    	}
    	else if(fFd != -1)
    	{
    		Bool16 haveContent = ReadFileContent();
    		if(haveContent)
    		{
    			willRequestEvent = willRequestEvent | EV_WR;
    		}
    	}
   	}

    if(events & Task::kReadEvent)
    {
	    QTSS_Error theErr = this->RecvData();
	    if(theErr == QTSS_RequestArrived)
	    {
	        QTSS_Error ok = this->ProcessRequest();  
	        if(ok != QTSS_RequestFailed)
	        {
	        	willRequestEvent = willRequestEvent | EV_WR;
	        	//fSocket.RequestEvent(EV_WR);
	       	}
	       	else
	       	{
	       		willRequestEvent = willRequestEvent | EV_RE;
	       		//fSocket.RequestEvent(EV_RE);
	       	}
	        //return 0;
	    }
	    else if(theErr == QTSS_NoErr)
	    {
	    	willRequestEvent = willRequestEvent | EV_RE;
	        //fSocket.RequestEvent(EV_RE);
	        //return 0;
	    }
	    else if(theErr == EAGAIN)
	    {
	        fprintf(stderr, "%s %s[%d][0x%016lX] theErr == EAGAIN %u, %s \n", 
	            __FILE__, __PRETTY_FUNCTION__, __LINE__, (long)this, theErr, strerror(theErr));
	        willRequestEvent = willRequestEvent | EV_RE;
	        //fSocket.RequestEvent(EV_RE);
	        //return 0;
	    }
	    else
	    {
	        fprintf(stderr, "%s %s[%d][0x%016lX] theErr %u, %s \n", 
	            __FILE__, __PRETTY_FUNCTION__, __LINE__, (long)this, theErr, strerror(theErr));
	        Disconnect();    
	        return -1;
	    }   
    }

	if(willRequestEvent == 0)
	{
		// strange.
		// it will be never happened.
		fprintf(stdout, "%s %s[%d][0x%016lX] events=0x%08X\n",
			__FILE__, __PRETTY_FUNCTION__, __LINE__, (long)this, events);
		Disconnect();    
	    return -1;		
	}
	else
	{
		fSocket.RequestEvent(willRequestEvent);
	}
	
    return 0;
}

QTSS_Error  HTTPSession::RecvData()
{
    QTSS_Error theErr = QTSS_NoErr;
    StrPtrLen lastReceveid(fStrReceived);

    char* start_pos = NULL;
    UInt32 blank_len = 0;
    start_pos = (char*)fRequestBuffer;
    start_pos += lastReceveid.Len;
    blank_len = kRequestBufferSizeInBytes - lastReceveid.Len;

    UInt32 read_len = 0;
    theErr = fSocket.Read(start_pos, blank_len, &read_len);
    if(theErr != QTSS_NoErr)
    {
        fprintf(stderr, "%s %s[%d][0x%016lX] errno=%d, %s, recv %u, \n", 
            __FILE__, __PRETTY_FUNCTION__, __LINE__, (long)this, errno, strerror(errno), read_len);
        return theErr;
    }
    
    fprintf(stdout, "%s %s[%d][0x%016lX] recv %u, \n%s", 
        __FILE__, __PRETTY_FUNCTION__, __LINE__, (long)this, read_len, start_pos);

    fStrReceived.Len += read_len;
   
    bool check = IsFullRequest();
    if(check)
    {
        return QTSS_RequestArrived;
    }
    
    return QTSS_NoErr;
}

Bool16 HTTPSession::SendData()
{  	
    if(fStrRemained.Len <= 0)
    {
    	return true;
    }
    
    OS_Error theErr;
    UInt32 send_len = 0;
    theErr = fSocket.Send(fStrRemained.Ptr, fStrRemained.Len, &send_len);
    if(send_len > 0)
    {        
    	fprintf(stdout, "%s %s[%d][0x%016lX] send %u, return %u\n", 
        __FILE__, __PRETTY_FUNCTION__, __LINE__, (long)this, fStrRemained.Len, send_len);
        fStrRemained.Ptr += send_len;
        fStrRemained.Len -= send_len;
        ::memmove(fResponseBuffer, fStrRemained.Ptr, fStrRemained.Len);
        fStrRemained.Ptr = fResponseBuffer;        
    }
    else
    {
        fprintf(stderr, "%s %s[%d][0x%016lX] send %u, return %u, errno=%d, %s\n", 
            __FILE__, __PRETTY_FUNCTION__, __LINE__, (long)this, fStrRemained.Len, send_len, 
            errno, strerror(errno));
    }
    
    if(theErr == EAGAIN)
    {
        fprintf(stderr, "%s %s[%d][0x%016lX] theErr[%d] == EAGAIN[%d], errno=%d, %s \n", 
            __FILE__, __PRETTY_FUNCTION__, __LINE__, (long)this, theErr, EAGAIN, 
            errno, strerror(errno));
        // If we get this error, we are currently flow-controlled and should
        // wait for the socket to become writeable again
     //   fSocket.RequestEvent(EV_WR);
        //I don't understand, mutexes? where or which?
     //   this->ForceSameThread();    // We are holding mutexes, so we need to force
                                    // the same thread to be used for next Run()
		return false;
    }
    else if(theErr == EPIPE) //Connection reset by peer
    {
    	Disconnect();
    }
    else if(theErr == ECONNRESET) //Connection reset by peer
    {
        Disconnect();
    }

    return true;
}

bool  HTTPSession::IsFullRequest()
{
    fStrRequest.Ptr = fStrReceived.Ptr;
    fStrRequest.Len = 0;
    
    StringParser headerParser(&fStrReceived);
    while (headerParser.GetThruEOL(NULL))
    {        
        if (headerParser.ExpectEOL())
        {
            fStrRequest.Len = headerParser.GetDataParsedLen();
            return true;
        }
    }
    
    return false;
}


Bool16 HTTPSession::Disconnect()
{
    fprintf(stdout, "%s %s[%d][0x%016lX]: Signal kKillEvent \n", 
            __FILE__, __PRETTY_FUNCTION__, __LINE__, (long)this);             
    
    this->Signal(Task::kKillEvent);    
    
    return true;
}

QTSS_Error  HTTPSession::ProcessRequest()
{
    QTSS_Error theError;
    
	fRequest.Clear();
	theError = fRequest.Parse(&fStrRequest);   
	if(theError != QTSS_NoErr)
	{
		fprintf(stderr, "%s %s[%d][0x%016lX] HTTPRequest Parse error: %d\n", 
			__FILE__, __PRETTY_FUNCTION__, __LINE__, (long)this, theError);

		this->MoveOnRequest();
		return QTSS_RequestFailed;
	}

	Bool16 ok = false;	  
	switch(fRequest.fMethod)
	{
		case httpGetMethod:
			ok = ResponseGet();
			break;		
		default:
		   //unhandled method.
			fprintf(stderr, "%s %s[%d][0x%016lX] unhandled method: %d", 
					__FILE__, __PRETTY_FUNCTION__, __LINE__, (long)this, fRequest.fMethod);
			fStatusCode = qtssClientBadRequest;
			ResponseError(fStatusCode);
			break;		  
	}	

    if(ok)
    {
        this->MoveOnRequest();
    }
 
    if(!ok)
    {
        return QTSS_RequestFailed;
    }   
    
    return QTSS_NoErr;
}

Bool16 HTTPSession::ResponseGet()
{
	Bool16 ret = true;
	
	if(strncmp(fRequest.fAbsoluteURI.Ptr, URI_CMD, strlen(URI_CMD)) == 0)
	{
		ret = ResponseCmd();
		return ret;
	}	
	
	char absolute_uri[fRequest.fAbsoluteURI.Len + 1];
	strncpy(absolute_uri, fRequest.fAbsoluteURI.Ptr, fRequest.fAbsoluteURI.Len);
	absolute_uri[fRequest.fAbsoluteURI.Len] = '\0';
	fprintf(stdout, "%s: absolute_uri=%s\n", __FUNCTION__, absolute_uri);

	char* request_file = absolute_uri;
	if(strcmp(absolute_uri, "/") == 0)
	{
		request_file = "/index.html";
	}

	char abs_path[PATH_MAX];
	snprintf(abs_path, PATH_MAX-1, "%s%s", ROOT_PATH, request_file);
	abs_path[PATH_MAX-1] = '\0';		
	if(file_exist(abs_path))
	{
		ret = ResponseFile(abs_path);
	}
	else
	{
		ret = ResponseFileNotFound(request_file);
	}	

	return ret;

}

Bool16 HTTPSession::ReadFileContent()
{
	if(fFd == -1)
	{
		return false;
	}
	
	ssize_t count = read(fFd, fBuffer, kReadBufferSize);
	if(count < kReadBufferSize)
	{
		close(fFd);
		fFd = -1;
	}

	if(count <= 0)
	{
		return false;
	}

	fprintf(stdout, "%s %s[%d][0x%016lX] read %u, return %lu\n", 
        __FILE__, __PRETTY_FUNCTION__, __LINE__, (long)this, kReadBufferSize, count);

    fResponse.Set(fStrRemained.Ptr+fStrRemained.Len, kResponseBufferSizeInBytes-fStrRemained.Len);
    fResponse.Put(fBuffer, count);

    fStrResponse.Set(fResponse.GetBufPtr(), fResponse.GetBytesWritten());
    //append to fStrRemained
    fStrRemained.Len += fStrResponse.Len;  
    //clear previous response.
    fStrResponse.Set(fResponseBuffer, 0);
	
	
	return true;
}

Bool16 HTTPSession::ResponseCmdAddChannel()
{
	Bool16 ret = true;

	if(fRequest.fParamPairs == NULL)
	{
		char* request_file = "/add_channel.html";
		char abs_path[PATH_MAX];
		snprintf(abs_path, PATH_MAX-1, "%s%s", ROOT_PATH, request_file);
		abs_path[PATH_MAX-1] = '\0';	
		
		ret = ResponseFile(abs_path);
		return ret;
	}

	CHANNEL_T* channelp = (CHANNEL_T*)malloc(sizeof(CHANNEL_T));
	if(channelp == NULL)
	{
		return false;
	}
	memset(channelp, 0, sizeof(CHANNEL_T));

	DEQUE_NODE* nodep = fRequest.fParamPairs;
	while(nodep)
	{
		UriParam* paramp = (UriParam*)nodep->datap;
		if(strcmp(paramp->key, "channel_id") == 0)
		{
			channelp->channel_id = atoi(paramp->value);
		}
		else if(strcmp(paramp->key, "liveid") == 0)
		{
			strncpy(channelp->liveid, paramp->value, HASH_LEN);
		}
		else if(strcmp(paramp->key, "bitrate") == 0)
		{
			channelp->bitrate = atoi(paramp->value);
		}
		else if(strcmp(paramp->key, "channel_name") == 0)
		{
			strncpy(channelp->channel_name, paramp->value, MAX_CHANNEL_NAME-1);
		}
		else if(strcmp(paramp->key, "codec_ts") == 0)
		{
			channelp->codec_ts = atoi(paramp->value);
		}
		else if(strcmp(paramp->key, "codec_flv") == 0)
		{
			channelp->codec_flv = atoi(paramp->value);
		}
		else if(strcmp(paramp->key, "codec_mp4") == 0)
		{
			channelp->codec_mp4 = atoi(paramp->value);
		}
		else if(strcmp(paramp->key, "source") == 0)
		{
			u_int32_t ip = 0;
			u_int16_t port = 80;
			#define MAX_IP_LEN			16
			#define MAX_PORT_LEN		8
			char ip_str[MAX_IP_LEN];			
			char* temp = strstr(paramp->value, ":");
			if(temp == NULL)
			{				
				ip = inet_network(paramp->value);
			}
			else
			{
				int ip_len = temp - paramp->value;
				if(ip_len>MAX_IP_LEN-1)
				{
					ip_len = MAX_IP_LEN-1;
				}
				strncpy(ip_str, paramp->value, ip_len);
				ip_str[ip_len] = '\0';
				ip = inet_network(ip_str);

				temp ++;
				port = atoi(temp);
			}

			DEQUE_NODE* nodep = channel_find_source(channelp, ip);
			if(nodep == NULL)
			{
				channel_add_source(channelp, ip, port);
			}
		}
		
		
		if(nodep->nextp == fRequest.fParamPairs)
		{
			break;
		}
		nodep = nodep->nextp;
	}

	CHANNEL_T* findp = NULL;
	/*
	findp = g_channels.FindChannelById(channelp->channel_id);
	if(findp != NULL)
	{
		char reason[MAX_REASON_LEN] = "";
		snprintf(reason, MAX_REASON_LEN-1, "channel_id [%d] exist", channelp->channel_id);
		reason[MAX_REASON_LEN-1] = '\0';
		ResponseCmdResult("add_channel", "failure", reason);
		channel_release(channelp);
		channelp = NULL;
		return true;
	}
	*/
	findp = g_channels.FindChannelByHash(channelp->liveid);
	if(findp != NULL)
	{
		char reason[MAX_REASON_LEN] = "";
		snprintf(reason, MAX_REASON_LEN-1, "liveid [%s] exist", channelp->liveid);
		reason[MAX_REASON_LEN-1] = '\0';
		ResponseCmdResult("add_channel", "failure", reason);
		channel_release(channelp);
		channelp = NULL;
		return true;
	}
	
	int result = g_channels.AddChannel(channelp);
	if(result != 0)
	{
		char reason[MAX_REASON_LEN] = "";
		snprintf(reason, MAX_REASON_LEN-1, "AddChannel() internal failure");
		reason[MAX_REASON_LEN-1] = '\0';
		ResponseCmdResult("add_channel", "failure", reason);
		channel_release(channelp);
		channelp = NULL;
		return true;
	}

	if(channelp->source_list)
	{
		result = start_channel(channelp);
		if(result != 0)
		{
			char reason[MAX_REASON_LEN] = "";
			snprintf(reason, MAX_REASON_LEN-1, "start_channel() internal failure");
			reason[MAX_REASON_LEN-1] = '\0';
			ResponseCmdResult("add_source", "failure", reason);
			free(channelp);
			channelp = NULL;
			return true;
		}
	}
	
	result = g_channels.WriteConfig(ROOT_PATH"/channels.xml");
	if(result != 0)
	{
		char reason[MAX_REASON_LEN] = "";
		snprintf(reason, MAX_REASON_LEN-1, "WriteConfig() internal failure");
		reason[MAX_REASON_LEN-1] = '\0';
		ResponseCmdResult("add_channel", "failure", reason);
		channel_release(channelp);
		channelp = NULL;
		return true;
	}
	
	ResponseCmdResult("add_channel", "success", "");
	
	return ret;
}

Bool16 HTTPSession::ResponseCmdDelChannel()
{
	Bool16 ret = true;
	if(fRequest.fParamPairs == NULL)
	{
		char* request_file = "/del_channel.html";
		char abs_path[PATH_MAX];
		snprintf(abs_path, PATH_MAX-1, "%s%s", ROOT_PATH, request_file);
		abs_path[PATH_MAX-1] = '\0';	
		
		ret = ResponseFile(abs_path);
		return ret;
	}

	int channel_id = 0;
	char* liveid = NULL;
	DEQUE_NODE* nodep = fRequest.fParamPairs;
	while(nodep)
	{
		UriParam* paramp = (UriParam*)nodep->datap;
		if(strcmp(paramp->key, "channel_id") == 0)
		{
			channel_id = atoi(paramp->value);
		}
		else if(strcmp(paramp->key, "liveid") == 0)
		{
			liveid = paramp->value;
		}		
		
		if(nodep->nextp == fRequest.fParamPairs)
		{
			break;
		}
		nodep = nodep->nextp;
	}

	if(liveid == NULL)
	{
		ResponseCmdResult("del_channel", "failure", "param[liveid] need");	
		return true;
	}

	CHANNEL_T* channelp = g_channels.FindChannelByHash(liveid);
	if(channelp == NULL)
	{
		char reason[MAX_REASON_LEN] = "";
		snprintf(reason, MAX_REASON_LEN-1, "can not find liveid[%s]", liveid);
		reason[MAX_REASON_LEN-1] = '\0';
		ResponseCmdResult("del_channel", "failure", reason);
		return true;
	}
	
	if(channelp->source_list)
	{
		int result = stop_channel(channelp);
		if(result != 0)
		{
			char reason[MAX_REASON_LEN] = "";
			snprintf(reason, MAX_REASON_LEN-1, "stop_channel() internal failure");
			reason[MAX_REASON_LEN-1] = '\0';
			ResponseCmdResult("del_channel", "failure", reason);
			free(channelp);
			channelp = NULL;
			return true;
		}
	}

	int result = g_channels.DeleteChannel(liveid);
	if(result != 0)
	{
		ResponseCmdResult("del_channel", "failure", "DeleteChannel() internal failure");		
		return true;
	}
		
	result = g_channels.WriteConfig(ROOT_PATH"/channels.xml");
	if(result != 0)
	{
		ResponseCmdResult("del_channel", "failure", "WriteConfig() internal failure");
		return true;
	}
	
	ResponseCmdResult("del_channel", "success", "");
	
	return ret;
}

#if 0
Bool16 HTTPSession::ResponseCmdListChannel()
{
	Bool16 ret = true;	

	char* request_file = "/channels.xml";
	char abs_path[PATH_MAX];
	snprintf(abs_path, PATH_MAX-1, "%s%s", ROOT_PATH, request_file);
	abs_path[PATH_MAX-1] = '\0';	
	
	ret = ResponseFile(abs_path);
	
	return ret;
}
#endif

Bool16 HTTPSession::ResponseCmdListChannel()
{
	Bool16 ret = true;
	
	char buffer[1024*4];
	StringFormatter content(buffer, sizeof(buffer));
	content.Put("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
	content.Put("<channels>\n");

	DEQUE_NODE* channel_list = g_channels.GetChannels();
	DEQUE_NODE* nodep = channel_list;
	while(nodep)
	{
		CHANNEL_T* channelp = (CHANNEL_T*)nodep->datap;		
		content.PutFmtStr("\t<channel channel_id=\"%d\" liveid=\"%s\" bitrate=\"%d\" channel_name=\"%s\" "
			"codec_ts=\"%d\" codec_flv=\"%d\" codec_mp4=\"%d\">\n", 
			channelp->channel_id, channelp->liveid, channelp->bitrate, channelp->channel_name,
			channelp->codec_ts, channelp->codec_flv, channelp->codec_mp4);

		content.Put("\t\t<sources>\n");

		DEQUE_NODE* node2p = channelp->source_list;
		while(node2p)
		{
			SOURCE_T* sourcep = (SOURCE_T*)node2p->datap;

			struct in_addr in;
			in.s_addr = htonl(sourcep->ip);
			char* str_ip = inet_ntoa(in);
			
			content.PutFmtStr("\t\t\t<source ip=\"%s\" port=\"%d\">\n",
				str_ip, sourcep->port);
			content.Put("\t\t\t</source>\n");
			
			if(node2p->nextp == channelp->source_list)
			{
				break;
			}
			node2p = node2p->nextp;
		}
		
		content.Put("\t\t</sources>\n");

		content.Put("\t</channel>\n");
		
		if(nodep->nextp == channel_list)
		{
			break;
		}
		nodep = nodep->nextp;
	}
	content.Put("</channels>\n");

	ResponseContent(content.GetBufPtr(), content.GetBytesWritten(), CONTENT_TYPE_TEXT_XML);

	return ret;
}

Bool16 HTTPSession::ResponseCmdAddSource()
{
	Bool16 ret = true;
	if(fRequest.fParamPairs == NULL)
	{
		char* request_file = "/add_source.html";
		char abs_path[PATH_MAX];
		snprintf(abs_path, PATH_MAX-1, "%s%s", ROOT_PATH, request_file);
		abs_path[PATH_MAX-1] = '\0';	
		
		ret = ResponseFile(abs_path);
		return ret;
	}

	int channel_id = 0;
	char* liveid = NULL;
	char* ip_str = NULL;
	u_int32_t ip = 0;
	u_int16_t port = 0;
	DEQUE_NODE* nodep = fRequest.fParamPairs;
	while(nodep)
	{
		UriParam* paramp = (UriParam*)nodep->datap;
		if(strcmp(paramp->key, "channel_id") == 0)
		{
			channel_id = atoi(paramp->value);
		}
		else if(strcmp(paramp->key, "liveid") == 0)
		{
			liveid = paramp->value;
		}
		else if(strcmp(paramp->key, "ip") == 0)
		{
			ip_str = paramp->value;
			ip = inet_network(ip_str);
		}
		else if(strcmp(paramp->key, "port") == 0)
		{
			port = atoi(paramp->value);
		}
		
		if(nodep->nextp == fRequest.fParamPairs)
		{
			break;
		}
		nodep = nodep->nextp;
	}

	if(liveid == NULL)
	{
		ResponseCmdResult("add_source", "failure", "param[liveid] need");	
		return true;
	}
	if(ip_str == NULL)
	{
		ResponseCmdResult("add_source", "failure", "param[ip] need");
		return true;
	}
	if(ip == 0 || port == 0)
	{
		char reason[MAX_REASON_LEN] = "";
		snprintf(reason, MAX_REASON_LEN-1, "check ip[0x%08X] and port[%d]", ip, port);
		reason[MAX_REASON_LEN-1] = '\0';
		ResponseCmdResult("add_source", "failure", reason);
		return true;
	}

	CHANNEL_T* channelp = g_channels.FindChannelByHash(liveid);
	if(channelp == NULL)
	{
		char reason[MAX_REASON_LEN] = "";
		snprintf(reason, MAX_REASON_LEN-1, "can not find channel by liveid[%s]", liveid);
		reason[MAX_REASON_LEN-1] = '\0';
		ResponseCmdResult("add_source", "failure", reason);
		return true;
	}

	DEQUE_NODE* findp = channel_find_source(channelp, ip);
	if(findp != NULL)
	{
		char reason[MAX_REASON_LEN] = "";
		snprintf(reason, MAX_REASON_LEN-1, "source exist[%s]", ip_str);
		reason[MAX_REASON_LEN-1] = '\0';
		ResponseCmdResult("add_source", "failure", reason);
		return true;
	}
	
	int result = channel_add_source(channelp, ip, port);
	if(result != 0)
	{
		char reason[MAX_REASON_LEN] = "";
		snprintf(reason, MAX_REASON_LEN-1, "channel_add_source() internal failure");
		reason[MAX_REASON_LEN-1] = '\0';
		ResponseCmdResult("add_source", "failure", reason);
		free(channelp);
		channelp = NULL;
		return true;
	}

	result = g_channels.WriteConfig(ROOT_PATH"/channels.xml");
	if(result != 0)
	{
		char reason[MAX_REASON_LEN] = "";
		snprintf(reason, MAX_REASON_LEN-1, "WriteConfig() internal failure");
		reason[MAX_REASON_LEN-1] = '\0';
		ResponseCmdResult("add_source", "failure", reason);
		free(channelp);
		channelp = NULL;
		return true;
	}

	ResponseCmdResult("add_source", "success", "");	

	return ret;
}


Bool16 HTTPSession::ResponseCmdDelSource()
{
	Bool16 ret = true;
	if(fRequest.fParamPairs == NULL)
	{
		char* request_file = "/del_source.html";
		char abs_path[PATH_MAX];
		snprintf(abs_path, PATH_MAX-1, "%s%s", ROOT_PATH, request_file);
		abs_path[PATH_MAX-1] = '\0';	
		
		ret = ResponseFile(abs_path);
		return ret;
	}

	int channel_id = 0;
	char* liveid = NULL;
	char* ip_str = NULL;
	u_int32_t ip = 0;
	u_int16_t port = 0;
	DEQUE_NODE* nodep = fRequest.fParamPairs;
	while(nodep)
	{
		UriParam* paramp = (UriParam*)nodep->datap;
		if(strcmp(paramp->key, "channel_id") == 0)
		{
			channel_id = atoi(paramp->value);
		}
		else if(strcmp(paramp->key, "liveid") == 0)
		{
			liveid = paramp->value;
		}
		else if(strcmp(paramp->key, "ip") == 0)
		{
			ip_str = paramp->value;
			ip = inet_network(ip_str);
		}
		else if(strcmp(paramp->key, "port") == 0)
		{
			port = atoi(paramp->value);
		}
		
		if(nodep->nextp == fRequest.fParamPairs)
		{
			break;
		}
		nodep = nodep->nextp;
	}

	if(liveid == NULL)
	{
		ResponseCmdResult("del_source", "failure", "param[liveid] need");	
		return true;
	}
	if(ip_str == NULL)
	{
		ResponseCmdResult("del_source", "failure", "param[ip] need");
		return true;
	}	

	CHANNEL_T* channelp = g_channels.FindChannelByHash(liveid);
	if(channelp == NULL)
	{
		char reason[MAX_REASON_LEN] = "";
		snprintf(reason, MAX_REASON_LEN-1, "can not find channel by liveid[%s]", liveid);
		reason[MAX_REASON_LEN-1] = '\0';
		ResponseCmdResult("del_source", "failure", reason);
		return true;
	}

	DEQUE_NODE* findp = channel_find_source(channelp, ip);
	if(findp == NULL)
	{
		char reason[MAX_REASON_LEN] = "";
		snprintf(reason, MAX_REASON_LEN-1, "source not exist[%s]", ip_str);
		reason[MAX_REASON_LEN-1] = '\0';
		ResponseCmdResult("del_source", "failure", reason);
		return true;
	}
	
	channelp->source_list = deque_remove_node(channelp->source_list, findp);

	int result = g_channels.WriteConfig(ROOT_PATH"/channels.xml");
	if(result != 0)
	{
		char reason[MAX_REASON_LEN] = "";
		snprintf(reason, MAX_REASON_LEN-1, "WriteConfig() internal failure");
		reason[MAX_REASON_LEN-1] = '\0';
		ResponseCmdResult("del_source", "failure", reason);
		free(channelp);
		channelp = NULL;
		return true;
	}

	ResponseCmdResult("del_source", "success", ""); 

	return ret;
}


Bool16 HTTPSession::ResponseCmdListSource()
{
	Bool16 ret = true;
	if(fRequest.fParamPairs == NULL)
	{
		char* request_file = "/list_source.html";
		char abs_path[PATH_MAX];
		snprintf(abs_path, PATH_MAX-1, "%s%s", ROOT_PATH, request_file);
		abs_path[PATH_MAX-1] = '\0';	
		
		ret = ResponseFile(abs_path);
		return ret;
	}

	int channel_id = 0;
	char* liveid = NULL;
	DEQUE_NODE* nodep = fRequest.fParamPairs;
	while(nodep)
	{
		UriParam* paramp = (UriParam*)nodep->datap;
		if(strcmp(paramp->key, "channel_id") == 0)
		{
			channel_id = atoi(paramp->value);
		}
		else if(strcmp(paramp->key, "liveid") == 0)
		{
			liveid = paramp->value;
		}		
		
		if(nodep->nextp == fRequest.fParamPairs)
		{
			break;
		}
		nodep = nodep->nextp;
	}

	if(liveid == NULL)
	{
		ResponseCmdResult("list_source", "failure", "param[liveid] need");	
		return true;
	}

	CHANNEL_T* channelp = g_channels.FindChannelByHash(liveid);
	if(channelp == NULL)
	{
		char reason[MAX_REASON_LEN] = "";
		snprintf(reason, MAX_REASON_LEN-1, "can not find channel by liveid[%s]", liveid);
		reason[MAX_REASON_LEN-1] = '\0';
		ResponseCmdResult("list_source", "failure", reason);
		return true;
	}

	char buffer[1024];
	StringFormatter content(buffer, sizeof(buffer));
	content.Put("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
	content.PutFmtStr("<channel channel_id=\"%d\" liveid=\"%s\" bitrate=\"%d\" channel_name=\"%s\" "
			"codec_ts=\"%d\" codec_flv=\"%d\" codec_mp4=\"%d\">\n", 
			channelp->channel_id, channelp->liveid, channelp->bitrate, channelp->channel_name,
			channelp->codec_ts, channelp->codec_flv, channelp->codec_mp4);
	content.Put("\t<sources>\n");
	
	DEQUE_NODE* node2p = channelp->source_list;
	while(node2p)
	{
		SOURCE_T* sourcep = (SOURCE_T*)node2p->datap;

		struct in_addr in;
		in.s_addr = htonl(sourcep->ip);
		char* str_ip = inet_ntoa(in);
		
		content.PutFmtStr("\t\t<source ip=\"%s\" port=\"%d\">\n",
			str_ip, sourcep->port);
		content.Put("\t\t</source>\n");
		
		if(node2p->nextp == channelp->source_list)
		{
			break;
		}
		node2p = node2p->nextp;
	}
	content.Put("\t</sources>\n");
	content.Put("</channel>\n");

	ResponseContent(content.GetBufPtr(), content.GetBytesWritten(), CONTENT_TYPE_TEXT_XML);
	
	return ret;
}


Bool16 HTTPSession::ResponseCmdResult(char* cmd, char* result, char* reason)
{
	char	buffer[1024];
	StringFormatter content(buffer, sizeof(buffer));
	
	content.Put("<HTML>\n");
	content.Put("<BODY>\n");
	content.Put("<TABLE border=\"0\" cellspacing=\"2\">\n");

	content.Put("<TR>\n");	
	content.Put("<TH align=\"right\">\n");
	content.Put("cmd:\n");
	content.Put("</TH>\n");
	content.Put("<TD align=\"left\">\n");
	content.PutFmtStr("%s\n", cmd);
	content.Put("</TD>\n");	
	content.Put("</TR>\n");	
	
	content.Put("<TR>\n");	
	content.Put("<TH align=\"right\">\n");
	content.Put("result:\n");
	content.Put("</TH>\n");
	content.Put("<TD align=\"left\">\n");
	content.PutFmtStr("%s\n", result);
	content.Put("</TD>\n");	
	content.Put("</TR>\n");

	content.Put("<TR>\n");	
	content.Put("<TH align=\"right\">\n");
	content.Put("reason:\n");
	content.Put("</TH>\n");
	content.Put("<TD align=\"left\">\n");
	content.PutFmtStr("%s\n", reason);
	content.Put("</TD>\n");	
	content.Put("</TR>\n");

	content.Put("</TABLE>\n");		
	content.Put("</BODY>\n");
	content.Put("</HTML>\n");

    ResponseContent(content.GetBufPtr(), content.GetBytesWritten(), CONTENT_TYPE_TEXT_HTML);
    
	return true;
}

Bool16 HTTPSession::ResponseContent(char* content, int len, char* type)
{			
	fResponse.Set(fStrRemained.Ptr+fStrRemained.Len, kResponseBufferSizeInBytes-fStrRemained.Len);
	fResponse.Put("HTTP/1.0 200 OK\r\n");
	fResponse.PutFmtStr("Server: %s/%s\r\n", BASE_SERVER_NAME, BASE_SERVER_VERSION);
	fResponse.PutFmtStr("Content-Length: %d\r\n", len);
	//fResponse.PutFmtStr("Content-Type: %s; charset=utf-8\r\n", content_type);
    fResponse.PutFmtStr("Content-Type: %s", type);
    if(strcmp(type, CONTENT_TYPE_TEXT_HTML) == 0)
    {
    	fResponse.PutFmtStr(";charset=%s\r\n", CHARSET_UTF8);
    }
    else
    {
    	fResponse.Put("\r\n");
    }
	fResponse.Put("\r\n"); 
	fResponse.Put(content, len);
	
	fStrResponse.Set(fResponse.GetBufPtr(), fResponse.GetBytesWritten());
	//append to fStrRemained
	fStrRemained.Len += fStrResponse.Len;  
	//clear previous response.
	fStrResponse.Set(fResponseBuffer, 0);
	
	//SendData();
	
	return true;
}

Bool16 HTTPSession::ResponseCmd()
{
	Bool16 ret = true;
	if(strncmp(fRequest.fAbsoluteURI.Ptr, CMD_LIST_CHANNEL, strlen(CMD_LIST_CHANNEL)) == 0)
	{
		ret = ResponseCmdListChannel();
	}	
	else if(strncmp(fRequest.fAbsoluteURI.Ptr, CMD_ADD_CHANNEL, strlen(CMD_ADD_CHANNEL)) == 0)
	{
		ret = ResponseCmdAddChannel();		
	}
	else if(strncmp(fRequest.fAbsoluteURI.Ptr, CMD_DEL_CHANNEL, strlen(CMD_DEL_CHANNEL)) == 0)
	{
		ret = ResponseCmdDelChannel();
	}	
	else if(strncmp(fRequest.fAbsoluteURI.Ptr, CMD_LIST_SOURCE, strlen(CMD_LIST_SOURCE)) == 0)
	{
		ret = ResponseCmdListSource();
	}	
	else if(strncmp(fRequest.fAbsoluteURI.Ptr, CMD_ADD_SOURCE, strlen(CMD_ADD_SOURCE)) == 0)
	{
		ret = ResponseCmdAddSource();		
	}
	else if(strncmp(fRequest.fAbsoluteURI.Ptr, CMD_DEL_SOURCE, strlen(CMD_DEL_SOURCE)) == 0)
	{
		ret = ResponseCmdDelSource();
	}	
	
	return ret;
}

Bool16 HTTPSession::ResponseFile(char* abs_path)
{
	fFd = open(abs_path, O_RDONLY);
	if(fFd == -1)
	{
		return false;
	}
	off_t file_len = lseek(fFd, 0L, SEEK_END);	
	lseek(fFd, 0L, SEEK_SET);

	char* suffix = file_suffix(abs_path);
	char* content_type = content_type_by_suffix(suffix);
	
	fResponse.Set(fStrRemained.Ptr+fStrRemained.Len, kResponseBufferSizeInBytes-fStrRemained.Len);
    fResponse.Put("HTTP/1.0 200 OK\r\n");
    fResponse.PutFmtStr("Server: %s/%s\r\n", BASE_SERVER_NAME, BASE_SERVER_VERSION);
    fResponse.PutFmtStr("Content-Length: %ld\r\n", file_len);
    //fResponse.PutFmtStr("Content-Type: %s; charset=utf-8\r\n", content_type);
    fResponse.PutFmtStr("Content-Type: %s", content_type);
    if(strcmp(content_type, CONTENT_TYPE_TEXT_HTML) == 0)
    {
    	fResponse.PutFmtStr(";charset=%s\r\n", CHARSET_UTF8);
    }
    else
    {
    	fResponse.Put("\r\n");
    }
    fResponse.Put("\r\n"); 
    
    fStrResponse.Set(fResponse.GetBufPtr(), fResponse.GetBytesWritten());
    //append to fStrRemained
    fStrRemained.Len += fStrResponse.Len;  
    //clear previous response.
    fStrResponse.Set(fResponseBuffer, 0);	
	
	return true;
}

Bool16 HTTPSession::ResponseFileNotFound(char* absolute_uri)
{
	char	buffer[1024];
	StringFormatter content(buffer, sizeof(buffer));
	
	content.Put("<HTML>\n");
	content.Put("<BODY>\n");
	content.Put("<TABLE border=2>\n");

	content.Put("<TR>\n");	
	content.Put("<TD>\n");
	content.Put("URI NOT FOUND!\n");
	content.Put("</TD>\n");
	content.Put("<TD>\n");
	content.PutFmtStr("%s\n", absolute_uri);
	content.Put("</TD>\n");	
	content.Put("</TR>\n");

	content.Put("<TR>\n");	
	content.Put("<TD>\n");
	content.Put("your ip:");
	content.Put("</TD>\n");
	content.Put("<TD>\n");
	UInt32  remote_ip =  fSocket.GetRemoteAddr();
	content.PutFmtStr("%u.%u.%u.%u\n", 
		(remote_ip & 0xFF000000) >> 24,
		(remote_ip & 0x00FF0000) >> 16,
		(remote_ip & 0x0000FF00) >> 8,
		(remote_ip & 0x000000FF) >> 0
		);
	content.Put("</TD>\n");	
	content.Put("</TR>\n");
	
	content.Put("<TR>\n");	
	content.Put("<TD>\n");
	content.Put("your port:\n");
	content.Put("</TD>\n");
	content.Put("<TD>\n");
	content.PutFmtStr("%u\n", fSocket.GetRemotePort());	
	content.Put("</TD>\n");	
	content.Put("</TR>\n");


	content.Put("<TR>\n");	
	content.Put("<TD>\n");
	content.Put("server:\n");
	content.Put("</TD>\n");
	content.Put("<TD>\n");
	content.PutFmtStr("%s/%s\n", BASE_SERVER_NAME, BASE_SERVER_VERSION);	
	content.Put("</TD>\n");	
	content.Put("</TR>\n");

	content.Put("</TABLE>\n");		
	content.Put("</BODY>\n");
	content.Put("</HTML>\n");
        
    fResponse.Set(fStrRemained.Ptr+fStrRemained.Len, kResponseBufferSizeInBytes-fStrRemained.Len);
    fResponse.Put("HTTP/1.0 200 OK\r\n");
    fResponse.PutFmtStr("Server: %s/%s\r\n", BASE_SERVER_NAME, BASE_SERVER_VERSION);
    fResponse.PutFmtStr("Content-Length: %d\r\n", content.GetBytesWritten());
    fResponse.PutFmtStr("Content-Type: %s;charset=%s\r\n", CONTENT_TYPE_TEXT_HTML, CHARSET_UTF8);
    fResponse.Put("\r\n"); 
    fResponse.Put(content.GetBufPtr(), content.GetBytesWritten());

    fStrResponse.Set(fResponse.GetBufPtr(), fResponse.GetBytesWritten());
    //append to fStrRemained
    fStrRemained.Len += fStrResponse.Len;  
    //clear previous response.
    fStrResponse.Set(fResponseBuffer, 0);
    
    //SendData();
    
	return true;
}

Bool16 HTTPSession::ResponseError(QTSS_RTSPStatusCode status_code)
{
    StrPtrLen blank;
    blank.Set(fStrRemained.Ptr+fStrRemained.Len, kResponseBufferSizeInBytes-fStrRemained.Len);
    int response_len = 0;
    
    response_len = snprintf(blank.Ptr, blank.Len-1, template_response_http_error,
            HTTPProtocol::GetStatusCodeAsString(status_code)->Ptr,  
            HTTPProtocol::GetStatusCodeString(status_code)->Ptr,  
            BASE_SERVER_NAME,
            BASE_SERVER_VERSION); 

    fStrResponse.Set(blank.Ptr, response_len);
    
    //append to fStrRemained
    fStrRemained.Len += fStrResponse.Len;  

    //clear previous response.
    fStrResponse.Set(fResponseBuffer, 0);

    //bool ok = SendData();
    //return ok;
    return true;
}



void HTTPSession::MoveOnRequest()
{
    StrPtrLen   strRemained;
    strRemained.Set(fStrRequest.Ptr+fStrRequest.Len, fStrReceived.Len-fStrRequest.Len);
        
    ::memmove(fRequestBuffer, strRemained.Ptr, strRemained.Len);
    fStrReceived.Set(fRequestBuffer, strRemained.Len);
    fStrRequest.Set(fRequestBuffer, 0);
}




