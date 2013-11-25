//file: HTTPSession.cpp

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
       
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "BaseServer/StringParser.h"

#include "public.h"
#include "config.h"
#include "channel.h"
#include "HTTPSession.h"

#define BASE_SERVER_NAME 	"TeslaStreamingServer"
#define BASE_SERVER_VERSION "1.0"

#define CONTENT_TYPE_TEXT_PLAIN					"text/plain"
#define CONTENT_TYPE_TEXT_HTML					"text/html"
#define CONTENT_TYPE_TEXT_CSS					"text/CSS"
//#define CONTENT_TYPE_TEXT_XML					"text/xml"
#define CONTENT_TYPE_APPLICATION_JAVASCRIPT		"application/javascript"
#define CONTENT_TYPE_APPLICATION_JSON			"application/json"
#define CONTENT_TYPE_APPLICATION_XML			"application/xml" // text/xml ?
#define CONTENT_TYPE_APPLICATION_SWF			"application/x-shockwave-flash" 
// audio/video
//#define CONTENT_TYPE_APPLICATION_M3U8			"application/x-mpegURL"
#define CONTENT_TYPE_APPLICATION_M3U8			"application/vnd.apple.mpegurl"
#define CONTENT_TYPE_VIDEO_FLV					"video/x-flv" 
#define CONTENT_TYPE_VIDEO_MP4					"video/mp4"
#define CONTENT_TYPE_AUDIO_MPEG					"audio/mpeg"
#define CONTENT_TYPE_VIDEO_QUICKTIME			"video/quicktime"
#define CONTENT_TYPE_VIDEO_MP2T					"video/MP2T"
// images
#define CONTENT_TYPE_IMAGE_PNG					"image/png"
#define CONTENT_TYPE_IMAGE_JPEG					"image/jpeg"
#define CONTENT_TYPE_IMAGE_GIF					"image/gif"
#define CONTENT_TYPE_IMAGE_BMP					"image/bmp"
#define CONTENT_TYPE_IMAGE_ICO					"image/vnd.microsoft.icon"
#define CONTENT_TYPE_IMAGE_TIFF					"image/tiff"
#define CONTENT_TYPE_IMAGE_SVG					"image/svg+xml"
// archives
#define CONTENT_TYPE_APPLICATION_ZIP			"application/zip"
#define CONTENT_TYPE_APPLICATION_RAR			"application/x-rar-compressed"
#define CONTENT_TYPE_APPLICATION_MSDOWNLOAD		"application/x-msdownload"
#define CONTENT_TYPE_APPLICATION_CAB			"application/vnd.ms-cab-compressed"
// adobe
#define CONTENT_TYPE_APPLICATION_PDF			"application/pdf"
#define CONTENT_TYPE_IMAGE_PHOTOSHOP			"image/vnd.adobe.photoshop"
#define CONTENT_TYPE_APPLICATION_POSTSCRIPT		"application/postscript"
// ms office
#define CONTENT_TYPE_APPLICATION_MSWORD         "application/msword"
#define CONTENT_TYPE_APPLICATION_RTF	        "application/rtf"
#define CONTENT_TYPE_APPLICATION_EXCEL	        "application/vnd.ms-excel"
#define CONTENT_TYPE_APPLICATION_POWERPOINT     "application/vnd.ms-powerpoint"
// open office
#define CONTENT_TYPE_APPLICATION_ODT      		"application/vnd.oasis.opendocument.text"
#define CONTENT_TYPE_APPLICATION_ODS            "application/vnd.oasis.opendocument.spreadsheet"

// octet-stream
#define CONTENT_TYPE_APPLICATION_OCTET_STREAM	"application/octet-stream"

#define CHARSET_UTF8		"utf-8"

#define URI_CMD  			"/cmd/"
#define CMD_LIST_CHANNEL  	"/cmd/list_channel/"
#define CMD_ADD_CHANNEL  	"/cmd/add_channel/"
#define CMD_DEL_CHANNEL  	"/cmd/del_channel/"
#define URI_LIVESTREAM		"/livestream/"
#define LIVE_TS				"ts"
#define LIVE_FLV			"flv"
#define LIVE_MP4			"mp4"

#define MAX_REASON_LEN		256


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
	/*
	     'txt' => 'text/plain',
            'htm' => 'text/html',
            'html' => 'text/html',
            'php' => 'text/html',
            'css' => 'text/css',
            'js' => 'application/javascript',
            'json' => 'application/json',
            'xml' => 'application/xml',
            'swf' => 'application/x-shockwave-flash',
            'flv' => 'video/x-flv',

            // images
            'png' => 'image/png',
            'jpe' => 'image/jpeg',
            'jpeg' => 'image/jpeg',
            'jpg' => 'image/jpeg',
            'gif' => 'image/gif',
            'bmp' => 'image/bmp',
            'ico' => 'image/vnd.microsoft.icon',
            'tiff' => 'image/tiff',
            'tif' => 'image/tiff',
            'svg' => 'image/svg+xml',
            'svgz' => 'image/svg+xml',

            // archives
            'zip' => 'application/zip',
            'rar' => 'application/x-rar-compressed',
            'exe' => 'application/x-msdownload',
            'msi' => 'application/x-msdownload',
            'cab' => 'application/vnd.ms-cab-compressed',

            // audio/video
            'mp3' => 'audio/mpeg',
            'qt' => 'video/quicktime',
            'mov' => 'video/quicktime',

            // adobe
            'pdf' => 'application/pdf',
            'psd' => 'image/vnd.adobe.photoshop',
            'ai' => 'application/postscript',
            'eps' => 'application/postscript',
            'ps' => 'application/postscript',

            // ms office
            'doc' => 'application/msword',
            'rtf' => 'application/rtf',
            'xls' => 'application/vnd.ms-excel',
            'ppt' => 'application/vnd.ms-powerpoint',

            // open office
            'odt' => 'application/vnd.oasis.opendocument.text',
            'ods' => 'application/vnd.oasis.opendocument.spreadsheet',
        */
        
	if(suffix == NULL)
	{
		return CONTENT_TYPE_TEXT_HTML;
	}

	if(strcasecmp(suffix, ".txt") == 0)
	{
		return CONTENT_TYPE_TEXT_PLAIN;
	}
	else if(strcasecmp(suffix, ".htm") == 0)
	{
		return CONTENT_TYPE_TEXT_HTML;
	}
	else if(strcasecmp(suffix, ".html") == 0)
	{
		return CONTENT_TYPE_TEXT_HTML;
	}
	else if(strcasecmp(suffix, ".php") == 0)
	{
		return CONTENT_TYPE_TEXT_HTML;
	}
	else if(strcasecmp(suffix, ".css") == 0)
	{
		return CONTENT_TYPE_TEXT_CSS;
	}
	else if(strcasecmp(suffix, ".js") == 0)
	{
		return CONTENT_TYPE_APPLICATION_JAVASCRIPT;
	}
	else if(strcasecmp(suffix, ".json") == 0)
	{
		return CONTENT_TYPE_APPLICATION_JSON;
	}
	else if(strcasecmp(suffix, ".xml") == 0)
	{
		return CONTENT_TYPE_APPLICATION_XML;
	}
	else if(strcasecmp(suffix, ".swf") == 0)
	{
		return CONTENT_TYPE_APPLICATION_SWF;
	}
	else if(strcasecmp(suffix, ".m3u8") == 0)
	{
		return CONTENT_TYPE_APPLICATION_M3U8;
	}
	else if(strcasecmp(suffix, ".flv") == 0)
	{
		return CONTENT_TYPE_VIDEO_FLV;
	}
	else if(strcasecmp(suffix, ".mp4") == 0)
	{
		return CONTENT_TYPE_VIDEO_MP4;
	}
	else if(strcasecmp(suffix, ".mp3") == 0)
	{
		return CONTENT_TYPE_AUDIO_MPEG;
	}
	else if(strcasecmp(suffix, ".qt") == 0)
	{
		return CONTENT_TYPE_VIDEO_QUICKTIME;
	}
	else if(strcasecmp(suffix, ".mov") == 0)
	{
		return CONTENT_TYPE_VIDEO_QUICKTIME;
	}
	else if(strcasecmp(suffix, ".ts") == 0)
	{
		return CONTENT_TYPE_VIDEO_MP2T;
	}
    else if(strcasecmp(suffix, ".png") == 0)
	{
		return CONTENT_TYPE_IMAGE_PNG;
	}
	else if(strcasecmp(suffix, ".jpe") == 0)
	{
		return CONTENT_TYPE_IMAGE_JPEG;
	}
	else if(strcasecmp(suffix, ".jpeg") == 0)
	{
		return CONTENT_TYPE_IMAGE_JPEG;
	}
	else if(strcasecmp(suffix, ".jpg") == 0)
	{
		return CONTENT_TYPE_IMAGE_JPEG;
	}
	else if(strcasecmp(suffix, ".gif") == 0)
	{
		return CONTENT_TYPE_IMAGE_GIF;
	}
	else if(strcasecmp(suffix, ".bmp") == 0)
	{
		return CONTENT_TYPE_IMAGE_BMP;
	}
	else if(strcasecmp(suffix, ".ico") == 0)
	{
		return CONTENT_TYPE_IMAGE_ICO;
	}
	else if(strcasecmp(suffix, ".tiff") == 0)
	{
		return CONTENT_TYPE_IMAGE_TIFF;
	}
	else if(strcasecmp(suffix, ".tif") == 0)
	{
		return CONTENT_TYPE_IMAGE_TIFF;
	}
	else if(strcasecmp(suffix, ".svg") == 0)
	{
		return CONTENT_TYPE_IMAGE_SVG;
	}
	else if(strcasecmp(suffix, ".svgz") == 0)
	{
		return CONTENT_TYPE_IMAGE_SVG;
	}
    else if(strcasecmp(suffix, ".zip") == 0)
	{
		return CONTENT_TYPE_APPLICATION_ZIP;
	}
	else if(strcasecmp(suffix, ".rar") == 0)
	{
		return CONTENT_TYPE_APPLICATION_RAR;
	}
	else if(strcasecmp(suffix, ".exe") == 0)
	{
		return CONTENT_TYPE_APPLICATION_MSDOWNLOAD;
	}
	else if(strcasecmp(suffix, ".msi") == 0)
	{
		return CONTENT_TYPE_APPLICATION_MSDOWNLOAD;
	}
	else if(strcasecmp(suffix, ".cab") == 0)
	{
		return CONTENT_TYPE_APPLICATION_CAB;
	}
    else if(strcasecmp(suffix, ".pdf") == 0)
	{
		return CONTENT_TYPE_APPLICATION_PDF;
	}
	else if(strcasecmp(suffix, ".psd") == 0)
	{
		return CONTENT_TYPE_IMAGE_PHOTOSHOP;
	}
	else if(strcasecmp(suffix, ".ai") == 0)
	{
		return CONTENT_TYPE_APPLICATION_POSTSCRIPT;
	}
	else if(strcasecmp(suffix, ".eps") == 0)
	{
		return CONTENT_TYPE_APPLICATION_POSTSCRIPT;
	}
	else if(strcasecmp(suffix, ".ps") == 0)
	{
		return CONTENT_TYPE_APPLICATION_POSTSCRIPT;
	}
    else if(strcasecmp(suffix, ".doc") == 0)
	{
		return CONTENT_TYPE_APPLICATION_MSWORD;
	}
	else if(strcasecmp(suffix, ".rtf") == 0)
	{
		return CONTENT_TYPE_APPLICATION_RTF;
	}
	else if(strcasecmp(suffix, ".xls") == 0)
	{
		return CONTENT_TYPE_APPLICATION_EXCEL;
	}
	else if(strcasecmp(suffix, ".ppt") == 0)
	{
		return CONTENT_TYPE_APPLICATION_POWERPOINT;
	}
	else if(strcasecmp(suffix, ".odt") == 0)
	{
		return CONTENT_TYPE_APPLICATION_ODT;
	}
	else if(strcasecmp(suffix, ".ods") == 0)
	{
		return CONTENT_TYPE_APPLICATION_ODS;
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
	fprintf(stdout, "%s[0x%016lX] remote_ip=0x%08X, port=%u \n", 
		__PRETTY_FUNCTION__, (long)this,
		fSocket.GetRemoteAddr(), fSocket.GetRemotePort());
	fFd	= -1;
	fData = NULL;
	fDataPosition = 0;
    fStatusCode     = 0;
    this->SetThreadPicker(&Task::sBlockingTaskThreadPicker);
}

HTTPSession::~HTTPSession()
{
	if(fFd != -1)
	{
		close(fFd);
		fFd = -1;
	}
    fprintf(stdout, "%s[0x%016lX] remote_ip=0x%08X, port=%u \n", 
		__PRETTY_FUNCTION__, (long)this,
		fSocket.GetRemoteAddr(), fSocket.GetRemotePort());
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
    	// this will never happen.
    	fprintf(stdout, "%s[%d][0x%016lX][%ld] remote_ip=0x%08X, port=%u events=0x%08X, errno=%d, %s !!!\n", 
			__PRETTY_FUNCTION__, __LINE__, (long)this, pthread_self(),
			fSocket.GetRemoteAddr(), fSocket.GetRemotePort(), events,
			errno, strerror(errno));		
    	return 1;
    }
    else if(events & Task::kErrorEvent)
    {
    	// epoll_wait EPOLLERR or EPOLLHUP, man epoll_ctl
    	fprintf(stdout, "%s[%d][0x%016lX][%ld] remote_ip=0x%08X, port=%u events=0x%08X, errno=%d, %s\n", 
			__PRETTY_FUNCTION__, __LINE__, (long)this, pthread_self(),
			fSocket.GetRemoteAddr(), fSocket.GetRemotePort(), events,
			errno, strerror(errno));
		Disconnect();
    	return -1;
    }
    
    if(events & Task::kKillEvent)
    {
        fprintf(stdout, "%s[%d][0x%016lX] remote_ip=0x%08X, port=%u events=0x%08X\n", 
			__PRETTY_FUNCTION__, __LINE__, (long)this,
			fSocket.GetRemoteAddr(), fSocket.GetRemotePort(), events);
        return -1;
    } 

	int willRequestEvent = 0;

	if(events & Task::kUpdateEvent)
    {
    	fprintf(stdout, "%s: kUpdateEvent [0x%016lX][0x%016lX][%ld] remote_ip=0x%08X, port=%u\n", 
    		__PRETTY_FUNCTION__, (long)this->fDefaultThread, (long)this->fUseThisThread, pthread_self(),
    		fSocket.GetRemoteAddr(), fSocket.GetRemotePort());
    	QTSS_Error ok = ContinueLive();
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
    }
	
    if(events & Task::kWriteEvent)
    {
    	QTSS_Error theErr = SendData();    
		//if(!sendDone)
    	if(theErr == QTSS_NoErr)    	
    	{
    		willRequestEvent = willRequestEvent | EV_WR;
    	}
    	// sendDone
    	else if(theErr == QTSS_ResponseDone)
	    {
	    	if(fFd != -1)
	    	{
	    		Bool16 haveContent = ReadFileContent();
	    		if(haveContent)
	    		{
	    			willRequestEvent = willRequestEvent | EV_WR;
	    		}
	    	}
	    	else if(fData != NULL)
	    	{
	    		Bool16 haveContent = ReadSegmentContent();
	    		if(haveContent)
	    		{
	    			willRequestEvent = willRequestEvent | EV_WR;
	    		}
	    	}
	    	else
	    	{
	    		willRequestEvent = willRequestEvent | EV_RE;
	    	}
    	}
    	else if(theErr == EAGAIN)
    	{
    		willRequestEvent = willRequestEvent | EV_RE;
    	}
    	else
    	{
    		fprintf(stderr, "%s[%d][0x%016lX] remote_ip=0x%08X, port=%u, theErr=%u, %s \n", 
	            __PRETTY_FUNCTION__, __LINE__, (long)this, 
	            fSocket.GetRemoteAddr(), fSocket.GetRemotePort(),
	            theErr, strerror(theErr));
	        Disconnect();    
	        return -1;
    	}
   	}

    if(events & Task::kReadEvent)
    {
	    QTSS_Error theErr = this->RecvData();
	    if(theErr == QTSS_RequestArrived)
	    {
	        QTSS_Error ok = this->ProcessRequest(); 
	        if(ok == QTSS_NotPreemptiveSafe)
	        {
	        	//this->SetSignal(Task::kUpdateEvent);
	        	return -2;
	        }
	        else if(ok != QTSS_RequestFailed)
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
	    	/*
	       		 fprintf(stderr, "%s[%d][0x%016lX] remote_ip=0x%08X, port=%u, theErr == EAGAIN %u, %s \n", 
	            		__PRETTY_FUNCTION__, __LINE__, (long)this, 
	           		fSocket.GetRemoteAddr(), fSocket.GetRemotePort(),
	            		theErr, strerror(theErr));
	              */
	        willRequestEvent = willRequestEvent | EV_RE;
	        //fSocket.RequestEvent(EV_RE);
	        //return 0;
	    }
	    else
	    {
	        fprintf(stderr, "%s[%d][0x%016lX] remote_ip=0x%08X, port=%u, theErr=%u, %s \n", 
	            __PRETTY_FUNCTION__, __LINE__, (long)this, 
	            fSocket.GetRemoteAddr(), fSocket.GetRemotePort(),
	            theErr, strerror(theErr));
	        Disconnect();    
	        return -1;
	    }   
    }

	if(willRequestEvent == 0)
	{
		// it will never happen.
		fprintf(stdout, "%s[%d][0x%016lX] remote_ip=0x%08X, port=%u, willRequestEvent=0x%08X !!!\n",
			__PRETTY_FUNCTION__, __LINE__, (long)this, 
			fSocket.GetRemoteAddr(), fSocket.GetRemotePort(),
			willRequestEvent);
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
    if(theErr != OS_NoErr)
    {
        fprintf(stderr, "%s[%d][0x%016lX] remote_ip=0x%08X, port=%u, recv=%u, errno=%d, %s\n", 
            __PRETTY_FUNCTION__, __LINE__, (long)this, 
            fSocket.GetRemoteAddr(), fSocket.GetRemotePort(),
            read_len, errno, strerror(errno));
        return theErr;
    }
    
    //fprintf(stdout, "%s %s[%d][0x%016lX] recv %u, \n%s", 
    //    __FILE__, __PRETTY_FUNCTION__, __LINE__, (long)this, read_len, start_pos);

    fStrReceived.Len += read_len;
   
    bool check = IsFullRequest();
    if(check)
    {
        return QTSS_RequestArrived;
    }
    
    return QTSS_NoErr;
}

QTSS_Error HTTPSession::SendData()
{  	
    if(fStrRemained.Len <= 0)
    {
    	return QTSS_ResponseDone;
    }
    
    OS_Error theErr;
    UInt32 send_len = 0;
    theErr = fSocket.Send(fStrRemained.Ptr, fStrRemained.Len, &send_len);
    if(send_len > 0)
    {        
    	//fprintf(stdout, "%s %s[%d][0x%016lX] send %u, return %u\n", 
        //__FILE__, __PRETTY_FUNCTION__, __LINE__, (long)this, fStrRemained.Len, send_len);
        fStrRemained.Ptr += send_len;
        fStrRemained.Len -= send_len;
        ::memmove(fResponseBuffer, fStrRemained.Ptr, fStrRemained.Len);
        fStrRemained.Ptr = fResponseBuffer; 
        return QTSS_NoErr;
    }
    else
    {
        fprintf(stderr, "%s[%d][0x%016lX] remote_ip=0x%08X, port=%u, send %u done %u return %u, errno=%d, %s\n", 
            __PRETTY_FUNCTION__, __LINE__, (long)this, 
            fSocket.GetRemoteAddr(), fSocket.GetRemotePort(),
            fStrRemained.Len, send_len, theErr,
            errno, strerror(errno));
        return theErr;
    }

    #if 0
    if(theErr == EAGAIN)
    {
        fprintf(stderr, "%s[%d][0x%016lX] remote_ip=0x%08X, port=%u, theErr[%d] == EAGAIN[%d], errno=%d, %s \n", 
            __PRETTY_FUNCTION__, __LINE__, (long)this, 
            fSocket.GetRemoteAddr(), fSocket.GetRemotePort(),
            theErr, EAGAIN, 
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
    #endif
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
		fStatusCode = httpBadRequest;
		theError = ResponseError(fStatusCode);
		
		this->MoveOnRequest();
		//return QTSS_RequestFailed;
		return theError;
	}
	  
	switch(fRequest.fMethod)
	{
		case httpGetMethod:
			theError = ResponseGet();
			break;
		case httpHeadMethod:
			theError = ResponseGet();
			break;
		default:
		   //unhandled method.
			fprintf(stderr, "%s %s[%d][0x%016lX] unhandled method: %d", 
					__FILE__, __PRETTY_FUNCTION__, __LINE__, (long)this, fRequest.fMethod);
			fStatusCode = httpBadRequest;
			theError = ResponseError(fStatusCode);
			//theError = QTSS_RequestFailed;
			break;		  
	}	

    //if(theError == QTSS_NoErr)
    {
        this->MoveOnRequest();
    }
     
    return theError;
}

QTSS_Error HTTPSession::ResponseGet()
{
	QTSS_Error ret = QTSS_NoErr;

	char absolute_uri[fRequest.fAbsoluteURI.Len + 1];
	strncpy(absolute_uri, fRequest.fAbsoluteURI.Ptr, fRequest.fAbsoluteURI.Len);
	absolute_uri[fRequest.fAbsoluteURI.Len] = '\0';
	//fprintf(stdout, "%s[%d][0x%016lX][%ld] remote_ip=0x%08X, port=%u absolute_uri=%s\n", 
	//		__PRETTY_FUNCTION__, __LINE__, (long)this, pthread_self(),
	//		fSocket.GetRemoteAddr(), fSocket.GetRemotePort(), absolute_uri);	
	
	// parser range if any.
	fHaveRange = false;
	fRangeStart = 0;
	fRangeStop = -1;
	StrPtrLen range = fRequest.fFieldValues[httpRangeHeader];	
	if(range.Len > 0)
	{
		fHaveRange = true;
		
		StringParser parser(&range);
		parser.ConsumeUntil(NULL, '=');
		parser.Expect('=');
		StrPtrLen start;
		parser.ConsumeUntil(&start, '-');
		fRangeStart = atol(start.Ptr);
		parser.Expect('-');
		if(parser.GetDataRemaining() > 0)
		{
			fRangeStop = atol(parser.GetCurrentPosition());
		}
		else
		{
			fRangeStop = -1;
		}
	}
	
	if(strncmp(fRequest.fAbsoluteURI.Ptr, URI_CMD, strlen(URI_CMD)) == 0)
	{
		ret = ResponseCmd();
		return ret;
	}
	else if(strncmp(fRequest.fAbsoluteURI.Ptr, URI_LIVESTREAM, strlen(URI_LIVESTREAM)) == 0)
	{
		ret = ResponseLive();
		return ret;
	}

	char* request_file = absolute_uri;
	if(strcmp(absolute_uri, "/") == 0)
	{
		request_file = "/index.html";
	}

	char abs_path[PATH_MAX];
	snprintf(abs_path, PATH_MAX-1, "%s%s", g_config.work_path, request_file);
	abs_path[PATH_MAX-1] = '\0';		
	if(file_exist(abs_path))
	{
		ret = ResponseFile(abs_path);
	}
	else
	{
		//ret = ResponseFileNotFound(request_file);
		ret = ResponseError(httpNotFound);		
	}	

	return ret;

}

Bool16 HTTPSession::ReadSegmentContent()
{
	if(!fHttpClientSession->Valid())
	{		
		return false;
	}

	int remain_len = fRangeStop + 1 - fDataPosition;
	int count = kReadBufferSize;
	if(count >= remain_len)
	{		
		count = remain_len;
	}

	memcpy(fBuffer, (char*)fData->datap+fDataPosition, count);
	fDataPosition = fDataPosition + count;
	if(fDataPosition > fRangeStop)
	{
		fData = NULL;
		fDataPosition = 0;
	}
	
	//fprintf(stdout, "%s %s[%d][0x%016lX] read %u, return %u\n", 
    //    __FILE__, __PRETTY_FUNCTION__, __LINE__, (long)this, kReadBufferSize, count);

	fResponse.Set(fStrRemained.Ptr+fStrRemained.Len, kResponseBufferSizeInBytes-fStrRemained.Len);
    fResponse.Put(fBuffer, count);

    fStrResponse.Set(fResponse.GetBufPtr(), fResponse.GetBytesWritten());
    //append to fStrRemained
    fStrRemained.Len += fStrResponse.Len;  
    //clear previous response.
    fStrResponse.Set(fResponseBuffer, 0);
	
	
	return true;     
}

Bool16 HTTPSession::ReadFileContent()
{
	if(fFd == -1)
	{
		return false;
	}

	int64_t read_len = kReadBufferSize;
	off_t current_pos = lseek(fFd, 0, SEEK_CUR);
	int64_t remain_len = fRangeStop+1-current_pos;
	if(remain_len<read_len)
	{
		read_len = remain_len;
	}
	ssize_t count = read(fFd, fBuffer, read_len);
	if(count < read_len)
	{
		close(fFd);
		fFd = -1;
	}
	else if(lseek(fFd, 0, SEEK_CUR) == fRangeStop+1)
	{
		close(fFd);
		fFd = -1;
	}

	if(count <= 0)
	{
		return false;
	}

	//fprintf(stdout, "%s %s[%d][0x%016lX] read %u, return %lu\n", 
    //    __FILE__, __PRETTY_FUNCTION__, __LINE__, (long)this, kReadBufferSize, count);

    fResponse.Set(fStrRemained.Ptr+fStrRemained.Len, kResponseBufferSizeInBytes-fStrRemained.Len);
    fResponse.Put(fBuffer, count);

    fStrResponse.Set(fResponse.GetBufPtr(), fResponse.GetBytesWritten());
    //append to fStrRemained
    fStrRemained.Len += fStrResponse.Len;  
    //clear previous response.
    fStrResponse.Set(fResponseBuffer, 0);	
	
	return true;
}

QTSS_Error HTTPSession::ResponseCmdAddChannel()
{
	QTSS_Error ret = QTSS_NoErr;

	if(fRequest.fParamPairs == NULL)
	{
		char* request_file = "/add_channel.html";
		char abs_path[PATH_MAX];
		snprintf(abs_path, PATH_MAX-1, "%s%s", g_config.work_path, request_file);
		abs_path[PATH_MAX-1] = '\0';	
		
		ret = ResponseFile(abs_path);
		return ret;
	}

	CHANNEL_T* channelp = (CHANNEL_T*)malloc(sizeof(CHANNEL_T));
	if(channelp == NULL)
	{
		ret = ResponseCmdResult("add_channel", "failure", "not enough memory!");
		return ret;
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
		else if(strcmp(paramp->key, "source") == 0 && strlen(paramp->value)>0)
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
		ret = ResponseCmdResult("add_channel", "failure", reason);
		channel_release(channelp);
		channelp = NULL;
		return ret;
	}
	
	int result = g_channels.AddChannel(channelp);
	if(result != 0)
	{
		char reason[MAX_REASON_LEN] = "";
		snprintf(reason, MAX_REASON_LEN-1, "AddChannel() internal failure");
		reason[MAX_REASON_LEN-1] = '\0';
		ret = ResponseCmdResult("add_channel", "failure", reason);
		channel_release(channelp);
		channelp = NULL;
		return ret;
	}

	if(channelp->source_list)
	{
		result = start_channel(channelp);
		if(result != 0)
		{
			char reason[MAX_REASON_LEN] = "";
			snprintf(reason, MAX_REASON_LEN-1, "start_channel() internal failure");
			reason[MAX_REASON_LEN-1] = '\0';
			ret = ResponseCmdResult("add_source", "failure", reason);
			free(channelp);
			channelp = NULL;
			return ret;
		}
	}
	
	result = g_channels.WriteConfig(g_config.channels_file);
	if(result != 0)
	{
		char reason[MAX_REASON_LEN] = "";
		snprintf(reason, MAX_REASON_LEN-1, "WriteConfig() internal failure");
		reason[MAX_REASON_LEN-1] = '\0';
		ret = ResponseCmdResult("add_channel", "failure", reason);
		channel_release(channelp);
		channelp = NULL;
		return ret;
	}
	
	ret = ResponseCmdResult("add_channel", "success", "");	
	return ret;
}

QTSS_Error HTTPSession::ResponseCmdDelChannel()
{
	QTSS_Error ret = QTSS_NoErr;
	if(fRequest.fParamPairs == NULL)
	{
		char* request_file = "/del_channel.html";
		char abs_path[PATH_MAX];
		snprintf(abs_path, PATH_MAX-1, "%s%s", g_config.work_path, request_file);
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
		ret = ResponseCmdResult("del_channel", "failure", "param[liveid] need");	
		return ret;
	}

	CHANNEL_T* channelp = g_channels.FindChannelByHash(liveid);
	if(channelp == NULL)
	{
		char reason[MAX_REASON_LEN] = "";
		snprintf(reason, MAX_REASON_LEN-1, "can not find liveid[%s]", liveid);
		reason[MAX_REASON_LEN-1] = '\0';
		ret = ResponseCmdResult("del_channel", "failure", reason);
		return ret;
	}
	
	if(channelp->source_list)
	{
		stop_channel(channelp);		
	}

	int result = g_channels.DeleteChannel(liveid);
	if(result != 0)
	{
		ret = ResponseCmdResult("del_channel", "failure", "DeleteChannel() internal failure");		
		return ret;
	}
		
	result = g_channels.WriteConfig(g_config.channels_file);
	if(result != 0)
	{
		ret = ResponseCmdResult("del_channel", "failure", "WriteConfig() internal failure");
		return ret;
	}
	
	ret = ResponseCmdResult("del_channel", "success", "");	
	return ret;
}

QTSS_Error HTTPSession::ResponseCmdListChannel()
{
	QTSS_Error ret = QTSS_NoErr;
	
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

	ResponseContent(content.GetBufPtr(), content.GetBytesWritten(), CONTENT_TYPE_APPLICATION_XML);

	return ret;
}

QTSS_Error HTTPSession::ResponseCmdResult(char* cmd, char* result, char* reason)
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

	content.Put("<TR>\n");	
	content.Put("<TD>\n");
	content.Put("server:\n");
	content.Put("</TD>\n");
	content.Put("<TD>\n");
	content.PutFmtStr("%s/%s\n", BASE_SERVER_NAME, BASE_SERVER_VERSION);	
	content.Put("</TD>\n");	
	content.Put("</TR>\n");

	content.Put("<TR>\n");	
	content.Put("<TD>\n");
	content.Put("time:");
	content.Put("</TD>\n");
	content.Put("<TD>\n");
	time_t now = time(NULL);
	char* now_str = ctime(&now);
	content.PutFmtStr("%s\n", now_str);
	content.Put("</TD>\n");	
	content.Put("</TR>\n");

	content.Put("</TABLE>\n");		
	content.Put("</BODY>\n");
	content.Put("</HTML>\n");

    ResponseContent(content.GetBufPtr(), content.GetBytesWritten(), CONTENT_TYPE_TEXT_HTML);
    
	return QTSS_NoErr;
}

Bool16 HTTPSession::ResponseContent(char* content, int len, char* type)
{			
	fResponse.Set(fStrRemained.Ptr+fStrRemained.Len, kResponseBufferSizeInBytes-fStrRemained.Len);
	fResponse.PutFmtStr("%s %s %s\r\n", 
    	HTTPProtocol::GetVersionString(http11Version)->Ptr,
    	HTTPProtocol::GetStatusCodeAsString(httpOK)->Ptr,
    	HTTPProtocol::GetStatusCodeString(httpOK)->Ptr);
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

	if(fRequest.fMethod == httpGetMethod)
	{
		fResponse.Put(content, len);		
	}
	else if(fRequest.fMethod == httpHeadMethod)
	{
		// do nothing.
	}
	
	fStrResponse.Set(fResponse.GetBufPtr(), fResponse.GetBytesWritten());
	//append to fStrRemained
	fStrRemained.Len += fStrResponse.Len;  
	//clear previous response.
	fStrResponse.Set(fResponseBuffer, 0);
	
	//SendData();
	
	return true;
}

QTSS_Error HTTPSession::ResponseCmd()
{
	QTSS_Error ret = QTSS_NoErr;
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
	else 
	{
		ret = ResponseError(httpBadRequest);
	}
	
	return ret;
}

QTSS_Error HTTPSession::ResponseLiveM3U8()
{
	QTSS_Error ret = QTSS_NoErr;

	fLiveRequest = kLiveM3U8;
	
	//char liveid[MAX_LIVE_ID];
	StringParser parser(&(fRequest.fRelativeURI));
	parser.ConsumeLength(NULL, strlen(URI_LIVESTREAM));
	StrPtrLen seg1;
	parser.ConsumeUntil(&seg1, '.');
	int len = seg1.Len;
	if(len >= MAX_LIVE_ID-1)
	{
		len = MAX_LIVE_ID-1;
	}
	memcpy(fLiveId, seg1.Ptr, len);
	fLiveId[len] = '\0';
	
	CHANNEL_T* channelp = g_channels.FindChannelByHash(fLiveId);
	if(channelp == NULL)
	{
		ret = ResponseError(httpNotFound);
		return ret;
	}

	char* param_type = LIVE_FLV;
	int   param_seq = -1;
	int   param_count = -1;
	DEQUE_NODE* nodep = fRequest.fParamPairs;
	while(nodep)
	{
		UriParam* paramp = (UriParam*)nodep->datap;
		if(strcasecmp(paramp->key, "codec") == 0)
		{
			param_type = paramp->value;
		}
		else if(strcasecmp(paramp->key, "seq") == 0)
		{
			param_seq = atoi(paramp->value);
		}
		else if(strcasecmp(paramp->key, "len") == 0)
		{
			param_count = atoi(paramp->value);
		}
		
		if(nodep->nextp == fRequest.fParamPairs)
		{
			break;	
		}
		nodep = nodep->nextp;
	}

	strncpy(fLiveType, param_type, MAX_LIVE_TYPE-1);
	fLiveType[MAX_LIVE_TYPE-1] = '\0';
	fLiveSeq = param_seq;
	fLiveLen = param_count;

	fHttpClientSession = NULL;
	MEMORY_T* memoryp = NULL;	
	if(strcasecmp(param_type, LIVE_TS) == 0)
	{
		fHttpClientSession = channelp->sessionp_ts;
		memoryp = channelp->memoryp_ts;
	}
	else if(strcasecmp(param_type, LIVE_FLV) == 0)
	{
		fHttpClientSession = channelp->sessionp_flv;
		memoryp = channelp->memoryp_flv;
	}
	else if(strcasecmp(param_type, LIVE_MP4) == 0)
	{
		fHttpClientSession = channelp->sessionp_mp4;
		memoryp = channelp->memoryp_mp4;
	}
	//if(memoryp == NULL)
	if(fHttpClientSession == NULL)
	{
		ret = ResponseError(httpNotFound);
		return ret;
	}

	fprintf(stdout, "%s: before fDefaultThread=0x%016lX, fUseThisThread=0x%016lX, %ld\n", 
		__PRETTY_FUNCTION__, (long)this->fDefaultThread, (long)this->fUseThisThread, pthread_self());
	TaskThread* threadp = fHttpClientSession->GetDefaultThread();
	this->SetSignal(Task::kUpdateEvent);
	this->SetDefaultThread(threadp);	
	this->SetTaskThread(threadp);
	fprintf(stdout, "%s: after  fDefaultThread=0x%016lX, fUseThisThread=0x%016lX, %ld\n", 
		__PRETTY_FUNCTION__, (long)this->fDefaultThread, (long)this->fUseThisThread, pthread_self());
	return QTSS_NotPreemptiveSafe;

}

QTSS_Error HTTPSession::ContinueLiveM3U8()
{
	QTSS_Error ret = QTSS_NoErr;

	if(!fHttpClientSession->Valid())
	{
		ret = ResponseError(httpGone);
		return ret;
	}

	MEMORY_T* memoryp = fHttpClientSession->GetMemory();
	//if(param_count == -1 && param_seq == -1)
	if(fLiveLen == -1 && fLiveSeq == -1)
	{
		if(memoryp->m3u8_num == 0)
		{
			ret = ResponseError(httpNotFound);
			return ret;
		}
		int index = memoryp->m3u8_index;
		index --;
		if(index < 0)
		{
			index = MAX_M3U8_NUM-1;		
		}
		
		ResponseContent((char*)memoryp->m3u8s[index].data.datap, memoryp->m3u8s[index].data.len, CONTENT_TYPE_APPLICATION_M3U8);
	}
	else
	{	
		if(memoryp->clip_num <= 0)
		{
			ResponseError(httpNotFound);
			return ret;
		}
		
		int index = memoryp->clip_index - 1;	
		if(index<0)
		{
			index = MAX_CLIP_NUM - 1;
		}
			
		int count = 0;
		while(1)
		{
			CLIP_T* onep = &(memoryp->clips[index]);
			//if(param_seq != -1 && (int64_t)onep->sequence < param_seq)
			if(fLiveSeq != -1 && (int64_t)onep->sequence < fLiveSeq)
			{	
				break;
			}
			
			count ++;			
			index --;
			if(index<0)
			{
				index = MAX_CLIP_NUM - 1;
			}

			if(count >= memoryp->clip_num || (fLiveLen !=-1 && count >= fLiveLen) )
			{
				break;
			}
		}

		index ++;
		if(index>=MAX_CLIP_NUM)
		{
			index = 0;
		}
		
		char m3u8_buffer[MAX_M3U8_CONTENT_LEN];
		StringFormatter content(m3u8_buffer, MAX_M3U8_CONTENT_LEN);	
		content.Put("#EXTM3U\n");
		content.PutFmtStr("#EXT-X-TARGETDURATION:%d\n", memoryp->target_duration);
		content.PutFmtStr("#EXT-X-MEDIA-SEQUENCE:%lu\n", memoryp->clips[index].sequence);
			
		int count2 = 0;
		while(1)
		{
			CLIP_T* onep = &(memoryp->clips[index]);
			content.PutFmtStr("#EXTINF:%u,\n", onep->inf);
			content.PutFmtStr("#EXT-X-BYTERANGE:%lu,\n", onep->byte_range);
			content.PutFmtStr("%s\n", onep->m3u8_relative_url);

			count2 ++;
			index ++;
			if(index>=MAX_CLIP_NUM)
			{
				index = 0;
			}
			
			if(count2>=count)
			{
				break;
			}
		}
		//content.PutTerminator();

		ResponseContent(m3u8_buffer, content.GetBytesWritten(), CONTENT_TYPE_APPLICATION_M3U8);
		
	}

	return ret;
}

QTSS_Error HTTPSession::ResponseLiveSegment()
{
	QTSS_Error ret = QTSS_NoErr;

	fLiveRequest = kLiveSegment;
	
	// /livestream/3702892333/78267cf4a7864a887540cf4af3c432dca3d52050/ts/2013/10/31/20131017T171949_03_20131031_140823_3059413.ts
	//char liveid[MAX_LIVE_ID];
	StringParser parser(&(fRequest.fRelativeURI));
	parser.ConsumeLength(NULL, strlen(URI_LIVESTREAM));
	StrPtrLen seg1;
	parser.ConsumeUntil(&seg1, '/');
	parser.Expect('/');
	StrPtrLen seg2;
	parser.ConsumeUntil(&seg2, '/');
	parser.Expect('/');	
	int len = seg2.Len;
	if(len >= MAX_LIVE_ID-1)
	{
		len = MAX_LIVE_ID-1;
	}
	memcpy(fLiveId, seg2.Ptr, len);
	fLiveId[len] = '\0';
	
	CHANNEL_T* channelp = g_channels.FindChannelByHash(fLiveId);
	if(channelp == NULL)
	{
		ret = ResponseError(httpNotFound);
		return ret;
	}	
	parser.ConsumeUntil(NULL, '.');
	parser.Expect('.');
	StrPtrLen seg3;
	parser.ConsumeUntil(&seg3, '?');
	int type_len = seg3.Len;
	if(type_len >= MAX_LIVE_TYPE-1)
	{
		type_len = MAX_LIVE_TYPE-1;
	}
	memcpy(fLiveType, seg3.Ptr, type_len);
	fLiveType[type_len] = '\0';

	fHttpClientSession = NULL;
	MEMORY_T* memoryp = NULL;
	char* type = seg3.Ptr;
	char* mime_type = CONTENT_TYPE_APPLICATION_OCTET_STREAM;
	if(strncasecmp(type, LIVE_TS, strlen(LIVE_TS)) == 0)
	{
		fHttpClientSession = channelp->sessionp_ts;
		memoryp = channelp->memoryp_ts;
		mime_type = CONTENT_TYPE_VIDEO_MP2T;
	}
	else if(strncasecmp(type, LIVE_FLV, strlen(LIVE_FLV)) == 0)
	{
		fHttpClientSession = channelp->sessionp_flv;
		memoryp = channelp->memoryp_flv;
		mime_type = CONTENT_TYPE_VIDEO_FLV;
	}
	else if(strncasecmp(type, LIVE_MP4, strlen(LIVE_MP4)) == 0)
	{
		fHttpClientSession = channelp->sessionp_mp4;
		memoryp = channelp->memoryp_mp4;
		mime_type = CONTENT_TYPE_VIDEO_MP4;
	}
	//if(memoryp == NULL)
	if(fHttpClientSession == NULL)
	{
		ret = ResponseError(httpNotFound);
		return ret;
	}

	fprintf(stdout, "%s: before fDefaultThread=0x%016lX, fUseThisThread=0x%016lX, %ld\n", 
		__PRETTY_FUNCTION__, (long)this->fDefaultThread, (long)this->fUseThisThread, pthread_self());
	TaskThread* threadp = fHttpClientSession->GetDefaultThread();
	this->SetSignal(Task::kUpdateEvent);
	this->SetDefaultThread(threadp);
	this->SetTaskThread(threadp);
	fprintf(stdout, "%s: after  fDefaultThread=0x%016lX, fUseThisThread=0x%016lX, %ld\n", 
		__PRETTY_FUNCTION__, (long)this->fDefaultThread, (long)this->fUseThisThread, pthread_self());
	return QTSS_NotPreemptiveSafe;
	
}

QTSS_Error HTTPSession::ContinueLiveSegment()
{
	QTSS_Error ret = QTSS_NoErr;

	if(!fHttpClientSession->Valid())
	{
		ret = ResponseError(httpGone);
		return ret;
	}

	char* mime_type = CONTENT_TYPE_APPLICATION_OCTET_STREAM;
	if(strncasecmp(fLiveType, LIVE_TS, strlen(LIVE_TS)) == 0)
	{
		mime_type = CONTENT_TYPE_VIDEO_MP2T;
	}
	else if(strncasecmp(fLiveType, LIVE_FLV, strlen(LIVE_FLV)) == 0)
	{
		mime_type = CONTENT_TYPE_VIDEO_FLV;
	}
	else if(strncasecmp(fLiveType, LIVE_MP4, strlen(LIVE_MP4)) == 0)
	{
		mime_type = CONTENT_TYPE_VIDEO_MP4;
	}
	
	MEMORY_T* memoryp = fHttpClientSession->GetMemory();
	CLIP_T* clipp = NULL;	
	int index = memoryp->clip_index - 1;	
	if(index<0)
	{
		index = MAX_CLIP_NUM - 1;
	}
		
	int count = 0;
	while(count <= memoryp->clip_num)
	{
		CLIP_T* onep = &(memoryp->clips[index]);
		if(strncmp(onep->relative_url, fRequest.fRelativeURI.Ptr, strlen(onep->relative_url)) == 0)
		{
			clipp = onep;
			break;
		}
		
		count ++;
		index --;
		if(index<0)
		{
			index = MAX_CLIP_NUM - 1;
		}
	}
	if(clipp == NULL)
	{
		ret = ResponseError(httpNotFound);
		return ret;
	}

	fData = &(clipp->data);
	fDataPosition = fRangeStart;
	if(fRangeStop == -1)
	{
		fRangeStop = fData->len-1;
	}

	HTTPStatusCode status_code = httpOK;	
	if(fHaveRange)
	{
		fprintf(stdout, "%s: range=%ld-%ld", __PRETTY_FUNCTION__, fRangeStart, fRangeStop);
		status_code = httpPartialContent;
	}
	
	fResponse.Set(fStrRemained.Ptr+fStrRemained.Len, kResponseBufferSizeInBytes-fStrRemained.Len);
	fResponse.PutFmtStr("%s %s %s\r\n", 
			HTTPProtocol::GetVersionString(http11Version)->Ptr,
			HTTPProtocol::GetStatusCodeAsString(status_code)->Ptr,
			HTTPProtocol::GetStatusCodeString(status_code)->Ptr);
    fResponse.PutFmtStr("Server: %s/%s\r\n", BASE_SERVER_NAME, BASE_SERVER_VERSION);
    fResponse.PutFmtStr("Accept-Ranges: bytes\r\n");	
    fResponse.PutFmtStr("Content-Length: %ld\r\n", fRangeStop+1-fRangeStart);    
    if(fHaveRange)
    {
    	//Content-Range: 1000-3000/5000
    	fResponse.PutFmtStr("Content-Range: bytes %ld-%ld/%ld\r\n", fRangeStart, fRangeStop, fData->len);    
    }
    //fResponse.PutFmtStr("Content-Type: %s; charset=utf-8\r\n", content_type);
    fResponse.PutFmtStr("Content-Type: %s\r\n", mime_type);    
    fResponse.Put("\r\n"); 

    if(fRequest.fMethod == httpGetMethod)
	{
		// do nothing.		
	}
	else if(fRequest.fMethod == httpHeadMethod)
	{
		fData = NULL;
	}
    
    fStrResponse.Set(fResponse.GetBufPtr(), fResponse.GetBytesWritten());
    //append to fStrRemained
    fStrRemained.Len += fStrResponse.Len;  
    //clear previous response.
    fStrResponse.Set(fResponseBuffer, 0);	
	
	return ret;
}

QTSS_Error HTTPSession::ResponseLive()
{
	QTSS_Error ret = QTSS_NoErr;
	//if(strcasestr(fRequest.fAbsoluteURI.Ptr, ".m3u8") != NULL)
	if(strcasestr(fRequest.fRequestPath, ".m3u8") != NULL)
	{	
		ret = ResponseLiveM3U8();
	}
	else 
	{
		ret = ResponseLiveSegment();		
	}
	
	return ret;
}

QTSS_Error HTTPSession::ContinueLive()
{
	QTSS_Error ret = QTSS_NoErr;
	if(fLiveRequest == kLiveM3U8)
	{	
		ret = ContinueLiveM3U8();
	}
	else if(fLiveRequest == kLiveSegment)
	{
		ret = ContinueLiveSegment();		
	}
	
	return ret;
}


QTSS_Error HTTPSession::ResponseFile(char* abs_path)
{
	fFd = open(abs_path, O_RDONLY);
	if(fFd == -1)
	{
		return false;
	}	
	
	off_t file_len = lseek(fFd, 0L, SEEK_END);	
	lseek(fFd, fRangeStart, SEEK_SET);
	if(fRangeStop == -1)
	{
		fRangeStop = file_len - 1;
	}
	
	char* suffix = file_suffix(abs_path);
	char* content_type = content_type_by_suffix(suffix);
	
	fResponse.Set(fStrRemained.Ptr+fStrRemained.Len, kResponseBufferSizeInBytes-fStrRemained.Len);

	HTTPStatusCode status_code = httpOK;	
	if(fHaveRange)
	{
		fprintf(stdout, "%s: range=%ld-%ld", __PRETTY_FUNCTION__, fRangeStart, fRangeStop);
		status_code = httpPartialContent;
	}
	/*
	Accept-Ranges:bytes
	Access-Control-Allow-Headers:Range
	Access-Control-Allow-Origin:*
	Connection:keep-alive
	Content-Length:224533099
	Content-Range:bytes 0-224533098/224533099
	Content-Type:video/mp4
	Date:Fri, 08 Nov 2013 05:02:21 GMT
	Last-Modified:Mon, 12 Aug 2013 03:23:06 GMT
	Server:funshion
	*/	
	fResponse.PutFmtStr("%s %s %s\r\n", 
			HTTPProtocol::GetVersionString(http11Version)->Ptr,
			HTTPProtocol::GetStatusCodeAsString(status_code)->Ptr,			
			HTTPProtocol::GetStatusCodeString(status_code)->Ptr);
    fResponse.PutFmtStr("Server: %s/%s\r\n", BASE_SERVER_NAME, BASE_SERVER_VERSION);
    fResponse.PutFmtStr("Accept-Ranges: bytes\r\n");	
    fResponse.PutFmtStr("Content-Length: %ld\r\n", fRangeStop+1-fRangeStart);    
    if(fHaveRange)
    {
    	//Content-Range: 1000-3000/5000
    	fResponse.PutFmtStr("Content-Range: bytes %ld-%ld/%ld\r\n", fRangeStart, fRangeStop, file_len);    
    }
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
    #if 0
    char* only_log = fResponse.GetAsCString();
    fprintf(stdout, "%s %s[%d][0x%016lX] %u, \n%s", 
        __FILE__, __PRETTY_FUNCTION__, __LINE__, (long)this, fResponse.GetBytesWritten(), only_log);
    delete only_log;
    #endif
	if(fRequest.fMethod == httpGetMethod)
	{
		// do nothing.		
	}
	else if(fRequest.fMethod == httpHeadMethod)
	{
		close(fFd);
		fFd = -1;
	}    
    
    fStrResponse.Set(fResponse.GetBufPtr(), fResponse.GetBytesWritten());
    //append to fStrRemained
    fStrRemained.Len += fStrResponse.Len;  
    //clear previous response.
    fStrResponse.Set(fResponseBuffer, 0);	
	
	return QTSS_NoErr;
}

QTSS_Error HTTPSession::ResponseError(HTTPStatusCode status_code)
{
	char	buffer[1024];
	StringFormatter content(buffer, sizeof(buffer));
	content.Put("<HTML>\n");
	content.Put("<BODY>\n");
	content.Put("<TABLE border=0>\n");

	content.Put("<TR>\n");	
	content.Put("<TD>\n");
	content.Put("url:\n");
	content.Put("</TD>\n");
	content.Put("<TD>\n");
	content.PutFmtStr("%s\n", fRequest.fRequestPath);
	content.Put("</TD>\n");	
	content.Put("</TR>\n");
	
	content.Put("<TR>\n");	
	content.Put("<TD>\n");
	content.Put("status:\n");
	content.Put("</TD>\n");
	content.Put("<TD>\n");
	content.PutFmtStr("%s\n", HTTPProtocol::GetStatusCodeAsString(status_code)->Ptr);
	content.Put("</TD>\n");	
	content.Put("</TR>\n");

	content.Put("<TR>\n");	
	content.Put("<TD>\n");
	content.Put("reason:");
	content.Put("</TD>\n");
	content.Put("<TD>\n");
	content.PutFmtStr("%s\n", HTTPProtocol::GetStatusCodeString(status_code)->Ptr);
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

	content.Put("<TR>\n");	
	content.Put("<TD>\n");
	content.Put("time:");
	content.Put("</TD>\n");
	content.Put("<TD>\n");
	time_t now = time(NULL);
	char* now_str = ctime(&now);
	content.PutFmtStr("%s\n", now_str);
	content.Put("</TD>\n");	
	content.Put("</TR>\n");

	content.Put("</TABLE>\n");		
	content.Put("</BODY>\n");
	content.Put("</HTML>\n");
    
    fResponse.Set(fStrRemained.Ptr+fStrRemained.Len, kResponseBufferSizeInBytes-fStrRemained.Len);
    fResponse.PutFmtStr("%s %s %s\r\n", 
    	HTTPProtocol::GetVersionString(http11Version)->Ptr,
    	HTTPProtocol::GetStatusCodeAsString(status_code)->Ptr,
    	HTTPProtocol::GetStatusCodeString(status_code)->Ptr);
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
    
    //Bool16 ret = SendData();    
	return QTSS_NoErr;

}

void HTTPSession::MoveOnRequest()
{
    StrPtrLen   strRemained;
    strRemained.Set(fStrRequest.Ptr+fStrRequest.Len, fStrReceived.Len-fStrRequest.Len);
        
    ::memmove(fRequestBuffer, strRemained.Ptr, strRemained.Len);
    fStrReceived.Set(fRequestBuffer, strRemained.Len);
    fStrRequest.Set(fRequestBuffer, 0);
}


