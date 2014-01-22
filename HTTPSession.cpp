//file: HTTPSession.cpp

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
       
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>

#include "BaseServer/StringParser.h"

#include "common.h"
#include "config.h"
#include "channel.h"
#include "HTTPSession.h"

#define CONTENT_TYPE_TEXT_PLAIN					"text/plain"
#define CONTENT_TYPE_TEXT_HTML					"text/html"
#define CONTENT_TYPE_TEXT_CSS					"text/CSS"
//#define CONTENT_TYPE_TEXT_XML					"text/xml"
#define CONTENT_TYPE_TEXT_XSL					"text/xsl"

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

#define URI_CMD  			"/macross/"
#define CMD_LIST_CHANNEL  	"list_channel"
#define CMD_ADD_CHANNEL  	"add_channel"
#define CMD_DEL_CHANNEL  	"del_channel"
#define CMD_UPDATE_CHANNEL 	"update_channel"
#define CMD_CHANNEL_STATUS 	"channel_status"
#define CMD_HTTP_SESSIONS 	"http_sessions"
#define CMD_SESSION_LIST 	"session_list"
#define CMD_QUERY_VERSION 	"queryversion"
#define CMD_QUERY_PROCESS 	"query_process"
#define CMD_QUERY_CHANNEL 	"query_channel"
#define CMD_QUERY_SESSION	"query_session"
#define CMD_GET_CONFIG		"get_config"
#define CMD_SET_CONFIG		"set_config"

#define MAX_REASON_LEN		256

#define RATE_LIMIT_INTERVAL	1000

int cmd_read_params(CMD_T* cmdp, DEQUE_NODE* param_list)
{
	DEQUE_NODE* nodep = param_list;
	while(nodep)
	{
		UriParam* paramp = (UriParam*)nodep->datap;
		if(strcmp(paramp->key, "cmd") == 0)
		{
			strncpy(cmdp->cmd, paramp->value, MAX_CMD_LEN-1);
			cmdp->cmd[MAX_CMD_LEN-1] = '\0';
		}
		else if(strcmp(paramp->key, "format") == 0)
		{
			cmdp->format = atoi(paramp->value);
		}		
		
		
		if(nodep->nextp == param_list)
		{
			break;
		}
		nodep = nodep->nextp;
	}

	return 0;
}

int channel_read_params(CHANNEL_T* channelp, DEQUE_NODE* param_list)
{
	DEQUE_NODE* nodep = param_list;
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
			int codec = atoi(paramp->value);
			if(codec == 1)
			{
				channelp->codec_ts = 1;
			}
			else
			{
				channelp->codec_ts = 0;
			}
		}
		else if(strcmp(paramp->key, "codec_flv") == 0)
		{
			int codec = atoi(paramp->value);
			if(codec == 1)
			{
				channelp->codec_flv = 1;
			}
			else
			{
				channelp->codec_flv = 0;
			}
		}
		else if(strcmp(paramp->key, "codec_mp4") == 0)
		{
			int codec = atoi(paramp->value);
			if(codec == 1)
			{
				channelp->codec_mp4 = 1;
			}
			else
			{
				channelp->codec_mp4 = 0;
			}
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

			DEQUE_NODE* nodep = channel_find_source(channelp, ip, port);
			if(nodep == NULL)
			{
				channel_add_source(channelp, ip, port);
			}
		}
		
		
		if(nodep->nextp == param_list)
		{
			break;
		}
		nodep = nodep->nextp;
	}

	return 0;
}

int params_get_liveid(DEQUE_NODE* param_list, char live_id[MAX_LIVE_ID])
{
	DEQUE_NODE* nodep = param_list;
	while(nodep)
	{
		UriParam* paramp = (UriParam*)nodep->datap;
		if(strcmp(paramp->key, "liveid") == 0)
		{
			strncpy(live_id, paramp->value, HASH_LEN);
			return 0;
		}
		
		
		if(nodep->nextp == param_list)
		{
			break;
		}
		nodep = nodep->nextp;
	}

	return -1;
}


int params_get_ip(DEQUE_NODE* param_list, char client_ip[MAX_IP_LEN])
{
	DEQUE_NODE* nodep = param_list;
	while(nodep)
	{
		UriParam* paramp = (UriParam*)nodep->datap;
		if(strcmp(paramp->key, "ip") == 0)
		{
			strncpy(client_ip, paramp->value, MAX_IP_LEN);
			return 0;
		}
		
		
		if(nodep->nextp == param_list)
		{
			break;
		}
		nodep = nodep->nextp;
	}

	return -1;
}


char* params_get_key(DEQUE_NODE* param_list, char* key)
{
	DEQUE_NODE* nodep = param_list;
	while(nodep)
	{
		UriParam* paramp = (UriParam*)nodep->datap;
		if(strcmp(paramp->key, key) == 0)
		{
			return paramp->value;
			return 0;
		}
		
		
		if(nodep->nextp == param_list)
		{
			break;
		}
		nodep = nodep->nextp;
	}

	return NULL;
}


SESSION_T* sessions_find_ip(u_int32_t ip)
{
	int index = 0;
	for(index=0; index<=g_http_session_pos; index++)
	{
		SESSION_T* sessionp = &g_http_sessions[index];	
		if(sessionp->remote_ip == ip)
		{
			return sessionp;
		}
	}
	return NULL;
}

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
	else if(strcasecmp(suffix, ".xsl") == 0)
	{
		return CONTENT_TYPE_TEXT_XSL;
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

u_int64_t network_rate(u_int64_t bytes, struct timeval* begin_time, struct timeval* end_time)
{
	time_t  diff_time = end_time->tv_sec - begin_time->tv_sec;
	diff_time = diff_time * 1000000;
	diff_time += end_time->tv_usec;
	diff_time -= begin_time->tv_usec;

	u_int64_t rate = bytes*8*1000000/diff_time;	
	return rate;
}

u_int64_t download_limit(struct timeval* begin_time, struct timeval* end_time, u_int64_t download_bytes, u_int64_t send_len)
{	
	if(g_config.download_limit < 0)
	{
		return send_len;
	}
	
	time_t  diff_time = end_time->tv_sec - begin_time->tv_sec;
	diff_time = diff_time * 1000000;
	diff_time += end_time->tv_usec;
	diff_time -= begin_time->tv_usec;

	u_int64_t actual_send_len = 0;
	u_int64_t full_rate_bytes = g_config.download_limit * diff_time / 8 / 1000000;
	if(full_rate_bytes <= download_bytes)
	{
		fprintf(stdout, "%s[%d]: download_limit=%ld, send_len=%ld, actual_send_len=%ld\n",
			__PRETTY_FUNCTION__, __LINE__, g_config.download_limit, send_len, actual_send_len);
		return actual_send_len;
	}
	
	actual_send_len = full_rate_bytes  - download_bytes;	
	if(send_len > actual_send_len)
	{
		fprintf(stdout, "%s[%d]: download_limit=%ld, send_len=%ld, actual_send_len=%ld\n",
			__PRETTY_FUNCTION__, __LINE__, g_config.download_limit, send_len, actual_send_len);
		return actual_send_len;
	}

	actual_send_len = send_len;
	fprintf(stdout, "%s[%d]: download_limit=%ld, send_len=%ld, actual_send_len=%ld\n",
			__PRETTY_FUNCTION__, __LINE__, g_config.download_limit, send_len, actual_send_len);
			
	return actual_send_len;	

}

HTTPSession::HTTPSession(SESSION_T* sessionp):
    fSocket(NULL, Socket::kNonBlockingSocketType),
    fStrReceived((char*)fRequestBuffer, 0),
    fStrRequest(fStrReceived),
    fStrResponse((char*)fResponseBuffer, 0),
    fStrRemained(fStrResponse),
    fResponse(NULL, 0)    
{
	fprintf(stdout, "%s[0x%016lX] remote_ip=0x%08X, port=%u \n", 
		__PRETTY_FUNCTION__, (long)this,
		sessionp->remote_ip, sessionp->remote_port);
		
	char task_name[32] = {'\0'};
	snprintf(task_name, 32, "s_0x%08X:%u", sessionp->remote_ip, sessionp->remote_port);
	task_name[32 - 1] = '\0';
	this->SetTaskName(task_name);

	fFd	= -1;
	fCmdBuffer = NULL;
	fCmdBufferSize = 0;
	fCmdContentLength = 0;
	fCmdContentPosition = 0;
    fHttpStatus     = 0;
    fContentLen = 0;    
    //this->SetThreadPicker(&Task::sShortTaskThreadPicker);
    this->SetThreadPicker(&Task::sBlockingTaskThreadPicker);
    fSessionp = sessionp;
    fSessionp->sessionp = this;
    fHttpClientSession = NULL;
	fMemory = NULL;
	fData	= NULL;
	fDataPosition = 0;
    fStatistics = NULL;
    gettimeofday(&(fSessionp->begin_time), NULL);    
}

HTTPSession::~HTTPSession()
{
	if(fStatistics != NULL)
	{
		fStatistics->session_num --;
		fStatistics = NULL;
	}

	if(fFd != -1)
	{
		close(fFd);
		fFd = -1;
	}
	if(fData != NULL)
	{
		fData = NULL;
	}
	if(fCmdBuffer != NULL)
	{
		free(fCmdBuffer);
		fCmdBuffer = NULL;
	}
    fprintf(stdout, "%s[0x%016lX] remote_ip=0x%08X, port=%u \n", 
		__PRETTY_FUNCTION__, (long)this,
		fSessionp->remote_ip, fSessionp->remote_port);
		
	gettimeofday(&(fSessionp->end_time), NULL);
	fSessionp->sessionp = NULL;
}

TCPSocket* HTTPSession::GetSocket() 
{ 
    return &fSocket;
}

#if 0
void 		HTTPSession::Log()
{	
	if(g_log != NULL)
	{
		char session_ip[MAX_IP_LEN] = {'\0'} ;
		struct in_addr s = {0};
		s.s_addr = fSessionp->remote_ip;
		inet_ntop(AF_INET, (const void *)&s, session_ip, MAX_IP_LEN);

		time_t now = time(NULL);
		char str_time[MAX_TIME_LEN] = {0};
		ctime_r(&now, str_time);
		str_time[strlen(str_time)-1] = '\0';

		char user_agent[fRequest.fFieldValues[httpUserAgentHeader].Len + 1];
		strncpy(user_agent, fRequest.fFieldValues[httpUserAgentHeader].Ptr, fRequest.fFieldValues[httpUserAgentHeader].Len);
		user_agent[fRequest.fFieldValues[httpUserAgentHeader].Len] = '\0';
		// remote_ip, remote_port, user, time, "request", http_status, content-length, "-", "user-agent", -, -
		fprintf(g_log, "%15s %5u %s [%s] \"%s\" %s %u \"%s\" \"%s\" %s %s\n", 
			session_ip, fSessionp->remote_port, "-",
			str_time, fRequest.fRequestPath, 
			HTTPProtocol::GetStatusCodeAsString(fHttpStatus)->Ptr,
			fContentLen, "-",
			user_agent,	"-", "-");
		//fflush(g_log);			
	}
}
#endif

void 		HTTPSession::Log()
{
	//fprintf(stdout, "%s[0x%016lX]: fDefaultThread=[0x%016lX], fUseThisThread=[0x%016lX]\n", 
	//	__PRETTY_FUNCTION__, (long)this, (long)this->fDefaultThread, (long)this->fUseThisThread);
	TaskThread* threadp = this->fDefaultThread;
	if(threadp == NULL)
	{
		return;
	}	

	struct timeval now = {};
	struct timezone tz = {};
	gettimeofday(&now, &tz);
	time_t second1 = threadp->fLogTime.tv_sec;
	time_t second2 = now.tv_sec;
	time_t day1 = (second1 - tz.tz_minuteswest*60)/(3600*24);
	time_t day2 = (second2 - tz.tz_minuteswest*60)/(3600*24);
	if(day1 != day2)
	{
		if(threadp->fLog == NULL)
		{
			threadp->fLogTime = now;
		}
		else
		{
			threadp->fLogTime.tv_sec = day2*3600*24+tz.tz_minuteswest*60;
			threadp->fLogTime.tv_usec = 0;
			fclose(threadp->fLog);
			threadp->fLog = NULL;
		}
		
		//threadp->fLogTime = now;		
		char FileName[PATH_MAX] = {'\0'};
		snprintf(FileName, PATH_MAX, "%s/sessions_%d_%d_%ld_%06ld.log", 
			g_config.log_path, getpid(), gettid(), threadp->fLogTime.tv_sec, threadp->fLogTime.tv_usec);
		FileName[PATH_MAX-1] = '\0';
		threadp->fLog = fopen(FileName, "a");
	}
	
	if(threadp->fLog != NULL)
	{
		char session_ip[MAX_IP_LEN] = {'\0'} ;
		struct in_addr s = {0};
		s.s_addr = fSessionp->remote_ip;
		inet_ntop(AF_INET, (const void *)&s, session_ip, MAX_IP_LEN);

		time_t now = time(NULL);
		char str_time[MAX_TIME_LEN] = {0};
		ctime_r(&now, str_time);
		str_time[strlen(str_time)-1] = '\0';

		char user_agent[fRequest.fFieldValues[httpUserAgentHeader].Len + 1];
		strncpy(user_agent, fRequest.fFieldValues[httpUserAgentHeader].Ptr, fRequest.fFieldValues[httpUserAgentHeader].Len);
		user_agent[fRequest.fFieldValues[httpUserAgentHeader].Len] = '\0';
		// remote_ip, remote_port, user, time, "request", http_status, content-length, "-", "user-agent", -, -
		fprintf(threadp->fLog, "%15s %5u %s [%s] \"%s\" %s %u \"%s\" \"%s\" %s %s\n", 
			session_ip, fSessionp->remote_port, "-",
			str_time, fRequest.fRequestPath, 
			HTTPProtocol::GetStatusCodeAsString(fHttpStatus)->Ptr,
			fContentLen, "-",
			user_agent,	"-", "-");
		//fflush(threadp->fLog);			
	}
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
        	Log();
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
    	else if(theErr == QTSS_NoMoreData)    	
    	{    		
    		//willRequestEvent = willRequestEvent | EV_WR;
    		this->SetSignal(Task::kWriteEvent);
    		return RATE_LIMIT_INTERVAL;
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
	    	else if(fCmdBuffer != NULL)
	    	{
	    		Bool16 haveContent = ReadCmdContent();
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
	        	Log();
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
    
    fSessionp->upload_bytes += read_len;
   
    bool check = IsFullRequest();
    if(check)
    {
        return QTSS_RequestArrived;
    }
    
    return QTSS_NoErr;
}

QTSS_Error HTTPSession::SendData()
{  	
	QTSS_Error ret = QTSS_NoErr;
	if(fStrRemained.Len <= 0)
    {
    	return QTSS_ResponseDone;
    }  	

	u_int64_t should_send_len = fStrRemained.Len;
		
	StrPtrLen user_agent = fRequest.fFieldValues[httpUserAgentHeader];
	if(user_agent.Len > 0 && strncmp(user_agent.Ptr, USER_AGENT, strlen(USER_AGENT))==0)
	{
		// download limit, no
		should_send_len = fStrRemained.Len;
	}
	else
	{
		// download limit, yes
	    struct timeval now_time;
		gettimeofday(&now_time, NULL);
		should_send_len = download_limit(&fSessionp->begin_time, &now_time, fSessionp->download_bytes, fStrRemained.Len);
		if(should_send_len < fStrRemained.Len)
		{
			ret = QTSS_NoMoreData;
		}
	}
    
    OS_Error theErr;
    UInt32 send_len = 0;
    //theErr = fSocket.Send(fStrRemained.Ptr, fStrRemained.Len, &send_len);
    theErr = fSocket.Send(fStrRemained.Ptr, should_send_len, &send_len);
    if(send_len > 0)
    {        
    	//fprintf(stdout, "%s %s[%d][0x%016lX] send %u, return %u\n", 
        //__FILE__, __PRETTY_FUNCTION__, __LINE__, (long)this, fStrRemained.Len, send_len);
        fStrRemained.Ptr += send_len;
        fStrRemained.Len -= send_len;
        ::memmove(fResponseBuffer, fStrRemained.Ptr, fStrRemained.Len);
        fStrRemained.Ptr = fResponseBuffer; 

        fSessionp->download_bytes += send_len;
        g_download_bytes += send_len;
        if(fStatistics != NULL)
        {
        	fStatistics->download_bytes += send_len;
        }

        if(fStrRemained.Len <= 0)
	    {
	    	ret = QTSS_ResponseDone;
	    }  	    
        return ret;
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

    fContentLen = 0;
	fRequest.Clear();
	theError = fRequest.Parse(&fStrRequest);   
	if(theError != QTSS_NoErr)
	{
		fprintf(stderr, "%s %s[%d][0x%016lX] HTTPRequest Parse error: %d\n", 
			__FILE__, __PRETTY_FUNCTION__, __LINE__, (long)this, theError);
		fHttpStatus = httpBadRequest;
		theError = ResponseError(fHttpStatus);
		
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
			fHttpStatus = httpBadRequest;
			theError = ResponseError(fHttpStatus);
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

	// parse range if any.
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
		if(fRangeStart < 0)
		{
			fHttpStatus = httpRequestRangeNotSatisfiable;
			ret = ResponseError(fHttpStatus);
			return ret;
		}
		
		parser.Expect('-');
		if(parser.GetDataRemaining() > 0)
		{
			fRangeStop = atol(parser.GetCurrentPosition());
			if(fRangeStop < 0 || fRangeStart>fRangeStop)
			{
				fHttpStatus = httpRequestRangeNotSatisfiable;
				ret = ResponseError(fHttpStatus);
				return ret;
			}
		}
		else
		{
			fRangeStop = -1;
		}

		
	}
	
	if(strncmp(fRequest.fAbsoluteURI.Ptr, URI_CMD, strlen(URI_CMD)) == 0)
	{
		fSessionp->session_type = fSessionp->session_type | SESSION_CMD;
		ret = ResponseCmd();
		return ret;
	}
	else if(strncmp(fRequest.fAbsoluteURI.Ptr, URI_LIVESTREAM, strlen(URI_LIVESTREAM)) == 0)
	{
		fSessionp->session_type = fSessionp->session_type | SESSION_LIVE;
		ret = ResponseLive();
		return ret;
	}

	fSessionp->session_type = fSessionp->session_type | SESSION_FILE;
	
	char* request_file = absolute_uri;
	if(strcmp(absolute_uri, "/") == 0)
	{
		request_file = "/index.html";
	}

	char abs_path[PATH_MAX];
	snprintf(abs_path, PATH_MAX, "%s%s", g_config.html_path, request_file);
	abs_path[PATH_MAX-1] = '\0';		
	if(file_exist(abs_path))
	{
		ret = ResponseFile(abs_path);
	}
	else
	{
		fHttpStatus = httpNotFound;
		ret = ResponseError(fHttpStatus);		
	}	

	return ret;

}

Bool16 HTTPSession::ReadCmdContent()
{
	int remain_len = fCmdContentLength - fCmdContentPosition;
	int count = kReadBufferSize;
	if(count >= remain_len)
	{		
		count = remain_len;
	}

	memcpy(fBuffer, (char*)fCmdBuffer+fCmdContentPosition, count);
	fCmdContentPosition = fCmdContentPosition + count;
	if(fCmdContentPosition >= fCmdContentLength)
	{
		free(fCmdBuffer);
		fCmdBuffer = NULL;
		fCmdBufferSize = 0;
		fCmdContentLength = 0;
		fCmdContentPosition = 0;
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

	CHANNEL_T* channelp = (CHANNEL_T*)malloc(sizeof(CHANNEL_T));
	if(channelp == NULL)
	{
		ret = ResponseCmdResult(CMD_ADD_CHANNEL, "error", "failure", "not enough memory!");
		return ret;
	}
	memset(channelp, 0, sizeof(CHANNEL_T));	
	channel_read_params(channelp, fRequest.fParamPairs);	

	if(strlen(channelp->liveid) == 0)
	{
		char* request_file = "/add_channel.html";
		char abs_path[PATH_MAX];
		snprintf(abs_path, PATH_MAX, "%s%s", g_config.html_path, request_file);
		abs_path[PATH_MAX-1] = '\0';	
		
		ret = ResponseFile(abs_path);

		channel_release(channelp);
		channelp = NULL;
		
		return ret;
	}

	CHANNEL_T* findp = NULL;	
	findp = g_channels.FindChannelByHash(channelp->liveid);
	if(findp != NULL)
	{
	#if 0
		char reason[MAX_REASON_LEN] = "";
		snprintf(reason, MAX_REASON_LEN, "liveid [%s] exist", channelp->liveid);
		reason[MAX_REASON_LEN-1] = '\0';
		ret = ResponseCmdResult("add_channel", "failure", reason);
		channel_release(channelp);
		channelp = NULL;
	#endif
		ret = ResponseCmdUpdateChannel(findp, channelp);
		return ret;
	}
	
	int result = g_channels.AddChannel(channelp);
	if(result != 0)
	{
		ret = ResponseCmdResult(CMD_ADD_CHANNEL, "error", "failure", "AddChannel() internal error");
		channel_release(channelp);
		channelp = NULL;
		return ret;
	}

	if(channelp->source_list)
	{
		result = start_channel(channelp);
		if(result != 0)
		{
			ret = ResponseCmdResult(CMD_ADD_CHANNEL, "error", "failure", "start_channel() internal error");
			free(channelp);
			channelp = NULL;
			return ret;
		}
	}
	
	result = g_channels.WriteConfig(g_config.channels_fullpath);
	if(result != 0)
	{
		ret = ResponseCmdResult(CMD_ADD_CHANNEL, "error", "failure", "WriteConfig() internal error");
		channel_release(channelp);
		channelp = NULL;
		return ret;
	}
	
	ret = ResponseCmdResult(CMD_ADD_CHANNEL, "ok", "success", "");	
	return ret;
}

QTSS_Error HTTPSession::ResponseCmdUpdateChannel(CHANNEL_T* findp, CHANNEL_T* channelp)
{
	QTSS_Error ret = QTSS_NoErr;
	
	findp->channel_id = channelp->channel_id;
	findp->bitrate	= channelp->bitrate;
	strcpy(findp->channel_name, channelp->channel_name);
	findp->source_list = channelp->source_list;

	findp->codec_ts = channelp->codec_ts;
	if(findp->sessionp_ts != NULL)
	{
		if(findp->codec_ts == 0 || findp->source_list == NULL)
		{
			// stop it
			HTTPClientSession* sessionp = findp->sessionp_ts;
			findp->sessionp_ts = NULL;
			sessionp->Stop();
		}
		else
		{
			// update source_list
			findp->sessionp_ts->UpdateSources(findp->source_list);
		}
	}
	else
	{
		if(findp->codec_ts == 1 && findp->source_list != NULL)
		{		
			// start it				
			HTTPClientSession* sessionp = new HTTPClientSession(findp, LIVE_TS);	
			if(sessionp != NULL)
			{
				sessionp->Start();
			}
		}
	}	

	findp->codec_flv = channelp->codec_flv;
	if(findp->sessionp_flv != NULL)
	{
		if(findp->codec_flv == 0 || findp->source_list == NULL)
		{
			// stop it
			HTTPClientSession* sessionp = findp->sessionp_flv;
			findp->sessionp_flv = NULL;
			sessionp->Stop();
		}
		else
		{
			// update source_list
			findp->sessionp_flv->UpdateSources(findp->source_list);
		}
	}
	else
	{
		if(findp->codec_flv == 1 && findp->source_list != NULL)
		{		
			// start it				
			HTTPClientSession* sessionp = new HTTPClientSession(findp, LIVE_FLV);	
			if(sessionp != NULL)
			{
				sessionp->Start();
			}
		}
	}	
	
	findp->codec_mp4 = channelp->codec_mp4;
	if(findp->sessionp_mp4 != NULL)
	{
		if(findp->codec_mp4 == 0 || findp->source_list == NULL)
		{
			// stop it
			HTTPClientSession* sessionp = findp->sessionp_mp4;
			findp->sessionp_mp4 = NULL;
			sessionp->Stop();
		}
		else
		{
			// update source_list
			findp->sessionp_mp4->UpdateSources(findp->source_list);
		}
	}
	else
	{
		if(findp->codec_mp4 == 1 && findp->source_list != NULL)
		{		
			// start it				
			HTTPClientSession* sessionp = new HTTPClientSession(findp, LIVE_MP4);	
			if(sessionp != NULL)
			{
				sessionp->Start();
			}
		}
	}

	if(channelp != NULL)
	{
		free(channelp);
		channelp = NULL;
	}
		
	int result = g_channels.WriteConfig(g_config.channels_fullpath);
	if(result != 0)
	{
		ret = ResponseCmdResult(CMD_UPDATE_CHANNEL, "error", "failure", "WriteConfig() internal error");		
		return ret;
	}
	
	ret = ResponseCmdResult(CMD_UPDATE_CHANNEL, "ok", "success", "");	
	return ret;
}

QTSS_Error HTTPSession::ResponseCmdDelChannel()
{
	QTSS_Error ret = QTSS_NoErr;

	CHANNEL_T* channelp = (CHANNEL_T*)malloc(sizeof(CHANNEL_T));
	if(channelp == NULL)
	{
		ret = ResponseCmdResult(CMD_DEL_CHANNEL, "error", "failure", "not enough memory");
		return ret;
	}
	memset(channelp, 0, sizeof(CHANNEL_T));	
	channel_read_params(channelp, fRequest.fParamPairs);	

	if(strlen(channelp->liveid) == 0)
	{
		char* request_file = "/del_channel.html";
		char abs_path[PATH_MAX];
		snprintf(abs_path, PATH_MAX, "%s%s", g_config.html_path, request_file);
		abs_path[PATH_MAX-1] = '\0';	
		
		ret = ResponseFile(abs_path);

		channel_release(channelp);
		channelp = NULL;
		
		return ret;
	}

	CHANNEL_T* findp = g_channels.FindChannelByHash(channelp->liveid);
	if(findp == NULL)
	{
		char reason[MAX_REASON_LEN] = "";
		snprintf(reason, MAX_REASON_LEN, "can not find liveid[%s]", channelp->liveid);
		reason[MAX_REASON_LEN-1] = '\0';
		//ret = ResponseCmdResult(CMD_DEL_CHANNEL, "error", "failure", reason);
		ret = ResponseCmdResult(CMD_DEL_CHANNEL, "ok", "non-exist", reason);

		channel_release(channelp);
		channelp = NULL;
		
		return ret;
	}
	
	if(findp->source_list)
	{
		stop_channel(findp);		
	}

	int result = g_channels.DeleteChannel(channelp->liveid);
	if(result != 0)
	{
		ret = ResponseCmdResult(CMD_DEL_CHANNEL, "error", "failure", "DeleteChannel() internal error");	

		channel_release(channelp);
		channelp = NULL;
		
		return ret;
	}
		
	result = g_channels.WriteConfig(g_config.channels_fullpath);
	if(result != 0)
	{
		ret = ResponseCmdResult(CMD_DEL_CHANNEL, "error", "failure", "WriteConfig() internal error");

		channel_release(channelp);
		channelp = NULL;
		
		return ret;
	}
	
	ret = ResponseCmdResult(CMD_DEL_CHANNEL, "ok", "success", "");

	channel_release(channelp);
	channelp = NULL;
		
	return ret;
}

QTSS_Error HTTPSession::ResponseCmdListChannel()
{
	QTSS_Error ret = QTSS_NoErr;

	int channel_num = g_channels.GetNum();
	if(channel_num <= 0)
	{
		channel_num = 1;
	}
	
	//char buffer[channel_num*2*1024];
	fCmdBufferSize = channel_num*2*1024;
	fCmdBuffer = (char*)malloc(fCmdBufferSize);
	if(fCmdBuffer == NULL)
	{
		fCmdBufferSize = 0;	
		fHttpStatus = httpInternalServerError;
		ret = ResponseError(fHttpStatus);
		return ret;
	}
	memset(fCmdBuffer, 0, fCmdBufferSize);
	StringFormatter content(fCmdBuffer, fCmdBufferSize);
	content.Put("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
	content.Put("<?xml-stylesheet type=\"text/xsl\" href=\"/list_channel.xsl\"?>\n");
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

	//ret = ResponseContent(content.GetBufPtr(), content.GetBytesWritten(), CONTENT_TYPE_APPLICATION_XML);	
	ret = ResponseHeader(content.GetBufPtr(), content.GetBytesWritten(), CONTENT_TYPE_APPLICATION_XML);

	fCmdContentLength = content.GetBytesWritten();	
	if(fRequest.fMethod == httpGetMethod)
	{
		fCmdContentPosition = 0;
	}
	else if(fRequest.fMethod == httpHeadMethod)
	{
		free(fCmdBuffer);
		fCmdBuffer = NULL;
		fCmdBufferSize = 0;
		fCmdContentLength = 0;
		fCmdContentPosition = 0;
	}

	return ret;
}

QTSS_Error HTTPSession::ResponseCmdQueryChannel()
{
	QTSS_Error ret = QTSS_NoErr;

	char live_id[MAX_LIVE_ID] = {'\0'};
	int get = params_get_liveid(fRequest.fParamPairs, live_id);
	if(get != 0)
	{
		char* request_file = "/query_channel.html";
		char abs_path[PATH_MAX];
		snprintf(abs_path, PATH_MAX, "%s%s", g_config.html_path, request_file);
		abs_path[PATH_MAX-1] = '\0';	
		ret = ResponseFile(abs_path);		
		return ret;
	}
	
	CHANNEL_T* channelp = g_channels.FindChannelByHash(live_id);
	if(channelp == NULL)
	{
		char reason[MAX_REASON_LEN] = "";
		snprintf(reason, MAX_REASON_LEN, "can not find liveid[%s]", live_id);
		reason[MAX_REASON_LEN-1] = '\0';	
		ret = ResponseCmdResult(CMD_QUERY_CHANNEL, "error", "failure", reason);
		return ret;
	}

	time_t	  start_time_ts  = 0;
	time_t	  start_time_flv = 0;
	time_t	  start_time_mp4 = 0;
	char 	  str_start_time_ts[MAX_TIME_LEN] = {0};
	char 	  str_start_time_flv[MAX_TIME_LEN] = {0};
	char 	  str_start_time_mp4[MAX_TIME_LEN] = {0};	
	u_int64_t newest_seq_ts  = 0;
	u_int64_t newest_seq_flv = 0;
	u_int64_t newest_seq_mp4 = 0;	
	if(channelp->memoryp_ts != NULL && channelp->sessionp_ts != NULL)
	{
		HTTPClientSession* clientp = channelp->sessionp_ts;
		start_time_ts = clientp->GetBeginTime()->tv_sec;
		ctime_r(&start_time_ts, str_start_time_ts);
		str_start_time_ts[strlen(str_start_time_ts)-1] = '\0';
		
		MEMORY_T* memoryp = channelp->memoryp_ts;
		int clip_index = memoryp->clip_index;
		clip_index --;
		if(clip_index < 0)
		{
			clip_index = g_config.max_clip_num - 1;
		}
		CLIP_T* clipp = &(memoryp->clips[clip_index]);
		newest_seq_ts = clipp->sequence;		
	}
	if(channelp->memoryp_flv != NULL && channelp->sessionp_flv != NULL)
	{
		HTTPClientSession* clientp = channelp->sessionp_flv;
		start_time_flv = clientp->GetBeginTime()->tv_sec;
		ctime_r(&start_time_flv, str_start_time_flv);
		str_start_time_flv[strlen(str_start_time_flv)-1] = '\0';
		
		MEMORY_T* memoryp = channelp->memoryp_flv;
		int clip_index = memoryp->clip_index;
		clip_index --;
		if(clip_index < 0)
		{
			clip_index = g_config.max_clip_num - 1;
		}
		CLIP_T* clipp = &(memoryp->clips[clip_index]);
		newest_seq_flv = clipp->sequence;		
	}
	if(channelp->memoryp_mp4 != NULL && channelp->sessionp_mp4 != NULL)
	{
		HTTPClientSession* clientp = channelp->sessionp_mp4;
		start_time_mp4 = clientp->GetBeginTime()->tv_sec;
		ctime_r(&start_time_mp4, str_start_time_mp4);
		str_start_time_mp4[strlen(str_start_time_mp4)-1] = '\0';
		
		MEMORY_T* memoryp = channelp->memoryp_mp4;
		int clip_index = memoryp->clip_index;
		clip_index --;
		if(clip_index < 0)
		{
			clip_index = g_config.max_clip_num - 1;
		}
		CLIP_T* clipp = &(memoryp->clips[clip_index]);
		newest_seq_mp4 = clipp->sequence;		
	}
	
	static time_t		last_query_time = 0;
	static u_int64_t	last_download_bytes_ts = 0;
	static u_int64_t	last_download_bytes_flv = 0;
	static u_int64_t	last_download_bytes_mp4 = 0;
	if(last_query_time == 0)
	{
		last_query_time = start_time_ts;
	}
	if(last_query_time == 0)
	{
		last_query_time = start_time_flv;
	}
	if(last_query_time == 0)
	{
		last_query_time = start_time_mp4;
	}
	if(last_query_time == 0)
	{
		last_query_time = g_start_time.tv_sec;
	}
	time_t now = time(NULL);
	time_t diff = now - last_query_time;
	if(diff == 0)
	{
		diff = 1;
	}
	u_int64_t download_rate_ts  = (channelp->statistics_ts.download_bytes - last_download_bytes_ts)*8 / diff;
	u_int64_t download_rate_flv = (channelp->statistics_flv.download_bytes - last_download_bytes_ts)*8 / diff;
	u_int64_t download_rate_mp4 = (channelp->statistics_mp4.download_bytes - last_download_bytes_ts)*8 / diff;

	char str_last_query_time[MAX_TIME_LEN] = {0};
	ctime_r(&last_query_time, str_last_query_time);
	str_last_query_time[strlen(str_last_query_time)-1] = '\0';
	char str_this_query_time[MAX_TIME_LEN] = {0};
	ctime_r(&now, str_this_query_time);
	str_this_query_time[strlen(str_this_query_time)-1] = '\0';
	
	char	buffer[4*1024];
	StringFormatter content(buffer, sizeof(buffer));
	content.PutFmtStr(
		"newest_sequence=%ld|%ld|%ld\n"
		"channel_start_time=%ld[%s]|%ld[%s]|%ld[%s]\n"	
		"total_session_num=%ld|%ld|%ld\n"
		"total_download_bytes=%ld|%ld|%ld\n"
		"last_query_time=%ld[%s]\n"
		"this_query_time=%ld[%s]\n"
		"last_download_bytes=%ld|%ld|%ld\n"
		"this_download_bytes=%ld|%ld|%ld\n"
		"this_download_rate=%ld|%ld|%ld bps\n", 
		newest_seq_ts, newest_seq_flv, newest_seq_mp4, 
		start_time_ts, str_start_time_ts, start_time_flv, str_start_time_flv, start_time_mp4, str_start_time_mp4,
		channelp->statistics_ts.session_num, channelp->statistics_flv.session_num, channelp->statistics_mp4.session_num,
		channelp->statistics_ts.download_bytes, channelp->statistics_flv.download_bytes, channelp->statistics_mp4.download_bytes,
		last_query_time, str_last_query_time,
		now, str_this_query_time,
		last_download_bytes_ts, last_download_bytes_flv, last_download_bytes_mp4,
		channelp->statistics_ts.download_bytes, channelp->statistics_flv.download_bytes, channelp->statistics_mp4.download_bytes,
		download_rate_ts, download_rate_flv, download_rate_mp4);	

	ResponseContent(content.GetBufPtr(), content.GetBytesWritten(), CONTENT_TYPE_TEXT_PLAIN);

	return ret;
}

QTSS_Error HTTPSession::ResponseCmdChannelStatus()
{
	QTSS_Error ret = QTSS_NoErr;
	
	int channel_num = g_channels.GetNum();
	if(channel_num <= 0)
	{
		channel_num = 1;
	}
	fCmdBufferSize = channel_num*4*1024;
	fCmdBuffer = (char*)malloc(fCmdBufferSize);
	if(fCmdBuffer == NULL)
	{
		fCmdBufferSize = 0;	
		fHttpStatus = httpInternalServerError;
		ret = ResponseError(fHttpStatus);
		return ret;
	}
	memset(fCmdBuffer, 0, fCmdBufferSize);
	StringFormatter content(fCmdBuffer, fCmdBufferSize);
	content.Put("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
	content.Put("<?xml-stylesheet type=\"text/xsl\" href=\"/channel_status.xsl\"?>\n");
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
		
		content.Put("\t\t<status>\n");
		if(channelp->memoryp_ts && channelp->sessionp_ts)
		{
			MEMORY_T* memoryp = channelp->memoryp_ts;
			HTTPClientSession* sessionp = channelp->sessionp_ts;
			int m3u8_index = memoryp->m3u8_index;
			m3u8_index --;
			if(m3u8_index < 0)
			{
				m3u8_index = MAX_M3U8_NUM - 1;
			}
			M3U8_T* m3u8p = &(memoryp->m3u8s[m3u8_index]);
			int clip_index = memoryp->clip_index;
			clip_index --;
			if(clip_index < 0)
			{
				clip_index = g_config.max_clip_num - 1;
			}
			CLIP_T* clipp = &(memoryp->clips[clip_index]);
			char str_m3u8_begin_time[MAX_TIME_LEN] = {0};
			char str_m3u8_end_time[MAX_TIME_LEN] = {0};
			char str_clip_begin_time[MAX_TIME_LEN] = {0};
			char str_clip_end_time[MAX_TIME_LEN] = {0};
			ctime_r(&m3u8p->begin_time, str_m3u8_begin_time);
			str_m3u8_begin_time[strlen(str_m3u8_begin_time)-1] = '\0';
			ctime_r(&m3u8p->end_time, str_m3u8_end_time);
			str_m3u8_end_time[strlen(str_m3u8_end_time)-1] = '\0';
			ctime_r(&clipp->begin_time, str_clip_begin_time);
			str_clip_begin_time[strlen(str_clip_begin_time)-1] = '\0';
			ctime_r(&clipp->end_time, str_clip_end_time);
			str_clip_end_time[strlen(str_clip_end_time)-1] = '\0';
			content.PutFmtStr("\t\t\t<%s source=\"%s\" m3u8_num=\"%d\" clip_num=\"%d[%lu]\" "
				"m3u8_begin_time=\"%ld[%s]\" m3u8_end_time=\"%ld[%s]\" "
				"clip_begin_time=\"%ld[%s]\" clip_end_time=\"%ld[%s]\" />\n",
				"tss", sessionp->GetSourceHost(), memoryp->m3u8_num, memoryp->clip_num, clipp->sequence,
				m3u8p->begin_time, str_m3u8_begin_time, m3u8p->end_time, str_m3u8_end_time,
				clipp->begin_time, str_clip_begin_time, clipp->end_time, str_clip_end_time);
		}
		if(channelp->memoryp_flv && channelp->sessionp_flv)
		{
			MEMORY_T* memoryp = channelp->memoryp_flv;
			HTTPClientSession* sessionp = channelp->sessionp_flv;
			int m3u8_index = memoryp->m3u8_index;
			m3u8_index --;
			if(m3u8_index < 0)
			{
				m3u8_index = MAX_M3U8_NUM - 1;
			}
			M3U8_T* m3u8p = &(memoryp->m3u8s[m3u8_index]);
			int clip_index = memoryp->clip_index;
			clip_index --;
			if(clip_index < 0)
			{
				clip_index = g_config.max_clip_num - 1;
			}
			CLIP_T* clipp = &(memoryp->clips[clip_index]);
			char str_m3u8_begin_time[MAX_TIME_LEN] = {0};
			char str_m3u8_end_time[MAX_TIME_LEN] = {0};
			char str_clip_begin_time[MAX_TIME_LEN] = {0};
			char str_clip_end_time[MAX_TIME_LEN] = {0};
			ctime_r(&m3u8p->begin_time, str_m3u8_begin_time);
			str_m3u8_begin_time[strlen(str_m3u8_begin_time)-1] = '\0';
			ctime_r(&m3u8p->end_time, str_m3u8_end_time);
			str_m3u8_end_time[strlen(str_m3u8_end_time)-1] = '\0';
			ctime_r(&clipp->begin_time, str_clip_begin_time);
			str_clip_begin_time[strlen(str_clip_begin_time)-1] = '\0';
			ctime_r(&clipp->end_time, str_clip_end_time);
			str_clip_end_time[strlen(str_clip_end_time)-1] = '\0';
			content.PutFmtStr("\t\t\t<%s source=\"%s\" m3u8_num=\"%d\" clip_num=\"%d[%lu]\" "
				"m3u8_begin_time=\"%ld[%s]\" m3u8_end_time=\"%ld[%s]\" "
				"clip_begin_time=\"%ld[%s]\" clip_end_time=\"%ld[%s]\" />\n",
				"flv", sessionp->GetSourceHost(), memoryp->m3u8_num, memoryp->clip_num, clipp->sequence,
				m3u8p->begin_time, str_m3u8_begin_time, m3u8p->end_time, str_m3u8_end_time,
				clipp->begin_time, str_clip_begin_time, clipp->end_time, str_clip_end_time);
		}
		if(channelp->memoryp_mp4 && channelp->sessionp_mp4)
		{
			MEMORY_T* memoryp = channelp->memoryp_mp4;
			HTTPClientSession* sessionp = channelp->sessionp_mp4;
			int m3u8_index = memoryp->m3u8_index;
			m3u8_index --;
			if(m3u8_index < 0)
			{
				m3u8_index = MAX_M3U8_NUM - 1;
			}
			M3U8_T* m3u8p = &(memoryp->m3u8s[m3u8_index]);
			int clip_index = memoryp->clip_index;
			clip_index --;
			if(clip_index < 0)
			{
				clip_index = g_config.max_clip_num - 1;
			}
			CLIP_T* clipp = &(memoryp->clips[clip_index]);
			char str_m3u8_begin_time[MAX_TIME_LEN] = {0};
			char str_m3u8_end_time[MAX_TIME_LEN] = {0};
			char str_clip_begin_time[MAX_TIME_LEN] = {0};
			char str_clip_end_time[MAX_TIME_LEN] = {0};
			ctime_r(&m3u8p->begin_time, str_m3u8_begin_time);
			str_m3u8_begin_time[strlen(str_m3u8_begin_time)-1] = '\0';
			ctime_r(&m3u8p->end_time, str_m3u8_end_time);
			str_m3u8_end_time[strlen(str_m3u8_end_time)-1] = '\0';
			ctime_r(&clipp->begin_time, str_clip_begin_time);
			str_clip_begin_time[strlen(str_clip_begin_time)-1] = '\0';
			ctime_r(&clipp->end_time, str_clip_end_time);
			str_clip_end_time[strlen(str_clip_end_time)-1] = '\0';
			content.PutFmtStr("\t\t\t<%s source=\"%s\" m3u8_num=\"%d\" clip_num=\"%d[%lu]\" "
				"m3u8_begin_time=\"%ld[%s]\" m3u8_end_time=\"%ld[%s]\" "
				"clip_begin_time=\"%ld[%s]\" clip_end_time=\"%ld[%s]\" />\n",
				"mp4", sessionp->GetSourceHost(), memoryp->m3u8_num, memoryp->clip_num, clipp->sequence,
				m3u8p->begin_time, str_m3u8_begin_time, m3u8p->end_time, str_m3u8_end_time,
				clipp->begin_time, str_clip_begin_time, clipp->end_time, str_clip_end_time);
		}		
		content.Put("\t\t</status>\n");

		content.Put("\t</channel>\n");
		
		if(nodep->nextp == channel_list)
		{
			break;
		}
		nodep = nodep->nextp;
	}
	content.Put("</channels>\n");

	//ret = ResponseContent(content.GetBufPtr(), content.GetBytesWritten(), CONTENT_TYPE_APPLICATION_XML);
	ret = ResponseHeader(content.GetBufPtr(), content.GetBytesWritten(), CONTENT_TYPE_APPLICATION_XML);

	fCmdContentLength = content.GetBytesWritten();	
	if(fRequest.fMethod == httpGetMethod)
	{
		fCmdContentPosition = 0;
	}
	else if(fRequest.fMethod == httpHeadMethod)
	{
		free(fCmdBuffer);
		fCmdBuffer = NULL;
		fCmdBufferSize = 0;
		fCmdContentLength = 0;
		fCmdContentPosition = 0;
	}

	return ret;
}


QTSS_Error HTTPSession::ResponseCmdQueryProcess()
{
	QTSS_Error ret = QTSS_NoErr;

	char str_start_time[MAX_TIME_LEN] = {0};
	ctime_r(&g_start_time.tv_sec, str_start_time);
	str_start_time[strlen(str_start_time)-1] = '\0';

	static time_t		last_query_time = 0;
	static u_int64_t	last_download_bytes = 0;
	if(last_query_time == 0)
	{
		last_query_time = g_start_time.tv_sec;
	}
	time_t now = time(NULL);
	time_t diff = now - last_query_time;
	if(diff == 0)
	{
		diff = 1;
	}
	u_int64_t download_rate = (g_download_bytes - last_download_bytes)*8 / diff;

	char str_last_query_time[MAX_TIME_LEN] = {0};
	ctime_r(&last_query_time, str_last_query_time);
	str_last_query_time[strlen(str_last_query_time)-1] = '\0';
	char str_this_query_time[MAX_TIME_LEN] = {0};
	ctime_r(&now, str_this_query_time);
	str_this_query_time[strlen(str_this_query_time)-1] = '\0';
	
	char	buffer[4*1024];
	StringFormatter content(buffer, sizeof(buffer));
	content.PutFmtStr(
		"process_start_time=%ld[%s]\n"		
		"total_download_bytes=%ld\n"
		"last_query_time=%ld[%s]\n"
		"this_query_time=%ld[%s]\n"
		"last_download_bytes=%ld\n"
		"this_download_bytes=%ld\n"
		"this_download_rate=%ld bps\n", 
		g_start_time.tv_sec, str_start_time, 
		g_download_bytes,
		last_query_time, str_last_query_time,
		now, str_this_query_time,
		last_download_bytes,
		g_download_bytes,
		download_rate);	

	ResponseContent(content.GetBufPtr(), content.GetBytesWritten(), CONTENT_TYPE_TEXT_PLAIN);

	last_query_time = now;
	last_download_bytes = g_download_bytes;
	
	return ret;
}


#if 0
QTSS_Error HTTPSession::ResponseCmdQuerySession()
{
	QTSS_Error ret = QTSS_NoErr;

	char client_ip[MAX_IP_LEN] = {'\0'};
	int get = params_get_ip(fRequest.fParamPairs, client_ip);
	if(get != 0)
	{
		char* request_file = "/query_session.html";
		char abs_path[PATH_MAX];
		snprintf(abs_path, PATH_MAX, "%s%s", g_config.work_path, request_file);
		abs_path[PATH_MAX-1] = '\0';	
		ret = ResponseFile(abs_path);		
		return ret;
	}

	u_int32_t ip = inet_addr(client_ip);
	SESSION_T* sessionp = sessions_find_ip(ip);
	if(sessionp == NULL)
	{
		char reason[MAX_REASON_LEN] = "";
		snprintf(reason, MAX_REASON_LEN, "can not find ip[%s]", client_ip);
		reason[MAX_REASON_LEN-1] = '\0';
		ret = ResponseCmdResult(CMD_QUERY_SESSION, "error", "failure", reason);
		return ret;
	}

	struct timeval now_time;
	gettimeofday(&now_time, NULL);

	struct timeval* until_timep = NULL;
	if(sessionp->end_time.tv_sec == 0)
	{
		until_timep = &now_time;
	}
	else
	{
		until_timep = &sessionp->end_time;
	}

	u_int64_t upload_rate = network_rate(sessionp->upload_bytes, &sessionp->begin_time, until_timep);
	u_int64_t download_rate = network_rate(sessionp->download_bytes, &sessionp->begin_time, until_timep);

	char session_ip[MAX_IP_LEN] = {'\0'} ;
	struct in_addr s = {0};
	s.s_addr = sessionp->remote_ip;
	inet_ntop(AF_INET, (const void *)&s, session_ip, MAX_IP_LEN);

	char	buffer[4*1024];
	StringFormatter content(buffer, sizeof(buffer));
	content.PutFmtStr("remote_ip=0x%08X[%s]\n"
		"remote_port=%d\n"
		"session_type=0x%08X\n"
		"upload_bytes=%ld\n"
		"download_bytes=%ld\n"
		"begin_time=%ld.%06ld\n"
		"end_time=%ld.%06ld\n"
		"now_time=%ld.%06ld\n"
		"upload_rate=%ld bps\n"
		"download_rate=%ld bps\n", 
		sessionp->remote_ip, session_ip, 
		sessionp->remote_port, 
		sessionp->session_type,
		sessionp->upload_bytes, 
		sessionp->download_bytes,
		sessionp->begin_time.tv_sec, sessionp->begin_time.tv_usec, 
		sessionp->end_time.tv_sec,   sessionp->end_time.tv_usec,
		now_time.tv_sec, now_time.tv_usec,
		upload_rate, 
		download_rate);	

	ResponseContent(content.GetBufPtr(), content.GetBytesWritten(), CONTENT_TYPE_TEXT_PLAIN);
	
	return ret;
}
#endif

QTSS_Error HTTPSession::ResponseCmdQuerySession()
{
	QTSS_Error ret = QTSS_NoErr;

	char client_ip[MAX_IP_LEN] = {'\0'};
	int get = params_get_ip(fRequest.fParamPairs, client_ip);
	if(get != 0)
	{
		char* request_file = "/query_session.html";
		char abs_path[PATH_MAX];
		snprintf(abs_path, PATH_MAX, "%s%s", g_config.html_path, request_file);
		abs_path[PATH_MAX-1] = '\0';	
		ret = ResponseFile(abs_path);		
		return ret;
	}

	u_int32_t ip = inet_addr(client_ip);

	int session_num = g_http_session_pos+1;	
	fCmdBufferSize = session_num*2*1024;
	fCmdBuffer = (char*)malloc(fCmdBufferSize);
	if(fCmdBuffer == NULL)
	{
		fCmdBufferSize = 0;	
		ret = ResponseError(httpInternalServerError);
		return ret;
	}
	memset(fCmdBuffer, 0, fCmdBufferSize);
	StringFormatter content(fCmdBuffer, fCmdBufferSize);
	content.Put("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
	content.Put("<?xml-stylesheet type=\"text/xsl\" href=\"/query_session.xsl\"?>\n");
	content.PutFmtStr("<sessions ip=\"%s\">\n", client_ip);

	struct timeval now_time;
	gettimeofday(&now_time, NULL);

	struct timeval* until_timep = NULL;	
	int index = 0;
	for(index=0; index<=g_http_session_pos; index++)
	{
		SESSION_T* sessionp = &g_http_sessions[index];	
		if(sessionp->sessionp ==  NULL && sessionp->begin_time.tv_sec == 0)
		{
			continue;
		}

		if(sessionp->remote_ip != ip)
		{
			continue;
		}

		if(sessionp->end_time.tv_sec == 0)
		{
			until_timep = &now_time;
		}
		else
		{
			until_timep = &sessionp->end_time;
		}

		u_int64_t upload_rate = network_rate(sessionp->upload_bytes, &sessionp->begin_time, until_timep);
		u_int64_t download_rate = network_rate(sessionp->download_bytes, &sessionp->begin_time, until_timep);

		char session_ip[MAX_IP_LEN] = {'\0'} ;
		struct in_addr s = {};
		s.s_addr = sessionp->remote_ip;
		inet_ntop(AF_INET, (const void *)&s, session_ip, MAX_IP_LEN);
		
		content.PutFmtStr("\t<session remote_ip=\"0x%08X[%s]\" remote_port=\"%d\" session_type=\"0x%08X\" "
			"upload_bytes=\"%ld\" download_bytes=\"%ld\" "
			"begin_time=\"%ld.%06ld\" end_time=\"%ld.%06ld\" now_time=\"%ld.%06ld\" "
			"upload_rate=\"%ld bps\" download_rate=\"%ld bps\">\n", 
			sessionp->remote_ip, session_ip, sessionp->remote_port, sessionp->session_type,
			sessionp->upload_bytes, sessionp->download_bytes,
			sessionp->begin_time.tv_sec, sessionp->begin_time.tv_usec, 
			sessionp->end_time.tv_sec,   sessionp->end_time.tv_usec,
			now_time.tv_sec, now_time.tv_usec,
			upload_rate, download_rate);		

		content.Put("\t</session>\n");
	}
	content.Put("</sessions>\n");
	
	//ret = ResponseContent(content.GetBufPtr(), content.GetBytesWritten(), CONTENT_TYPE_APPLICATION_XML);
	ret = ResponseHeader(content.GetBufPtr(), content.GetBytesWritten(), CONTENT_TYPE_APPLICATION_XML);

	fCmdContentLength = content.GetBytesWritten();	
	if(fRequest.fMethod == httpGetMethod)
	{
		fCmdContentPosition = 0;
	}
	else if(fRequest.fMethod == httpHeadMethod)
	{
		free(fCmdBuffer);
		fCmdBuffer = NULL;
		fCmdBufferSize = 0;
		fCmdContentLength = 0;
		fCmdContentPosition = 0;
	}	

	return ret;
}


QTSS_Error HTTPSession::ResponseCmdHttpSessions()
{
	QTSS_Error ret = QTSS_NoErr;

	int 	session_num_ongoing = 0;
	int 	session_num_ongoing_data = 0;	// with data flow
	int 	session_num_ongoing_null = 0; 	// no data flow
	int		session_num_stopped = 0;
	int 	session_num_stopped_data = 0;	// with data flow
	int 	session_num_stopped_null = 0; 	// no data flow
	
	struct timeval now_time;
	gettimeofday(&now_time, NULL);
	char str_now[MAX_TIME_LEN] = {0};
	ctime_r(&now_time.tv_sec, str_now);
	str_now[strlen(str_now)-1] = '\0';	
	
	int index = 0;
	for(index=0; index<=g_http_session_pos; index++)
	{
		SESSION_T* sessionp = &g_http_sessions[index];	
		if(sessionp->sessionp ==  NULL && sessionp->begin_time.tv_sec == 0)
		{
			continue;
		}

		if(sessionp->end_time.tv_sec == 0)
		{
			session_num_ongoing ++;
			if(sessionp->upload_bytes == 0 && sessionp->download_bytes == 0)
			{
				session_num_ongoing_null ++;
			}
			else
			{
				session_num_ongoing_data ++;
			}
		}
		else
		{
			session_num_stopped ++;
			if(sessionp->upload_bytes == 0 && sessionp->download_bytes == 0)
			{
				session_num_stopped_null ++;
			}
			else
			{
				session_num_stopped_data ++;
			}
		}
	}
		
	char	buffer[4*1024];
	StringFormatter content(buffer, sizeof(buffer));
	content.PutFmtStr(
		"time=%ld[%s]\n"		
		"session_num_ongoing=%d\n"
		"session_num_ongoing_data=%d\n"
		"session_num_ongoing_null=%d\n"
		"session_num_stopped=%d\n"
		"session_num_stopped_data=%d\n"
		"session_num_stopped_null=%d\n", 
		now_time.tv_sec, str_now, 
		session_num_ongoing,
		session_num_ongoing_data,
		session_num_ongoing_null,
		session_num_stopped,
		session_num_stopped_data,
		session_num_stopped_null);	

	ResponseContent(content.GetBufPtr(), content.GetBytesWritten(), CONTENT_TYPE_TEXT_PLAIN);

	return ret;
}


QTSS_Error HTTPSession::ResponseCmdSessionList()
{
	QTSS_Error ret = QTSS_NoErr;

	int session_num = g_http_session_pos+1;	
	fCmdBufferSize = session_num*2*1024;
	fCmdBuffer = (char*)malloc(fCmdBufferSize);
	if(fCmdBuffer == NULL)
	{
		fCmdBufferSize = 0;	
		ret = ResponseError(httpInternalServerError);
		return ret;
	}
	memset(fCmdBuffer, 0, fCmdBufferSize);
	StringFormatter content(fCmdBuffer, fCmdBufferSize);
	content.Put("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
	content.Put("<?xml-stylesheet type=\"text/xsl\" href=\"/session_status.xsl\"?>\n");
	content.Put("<sessions>\n");

	struct timeval now_time;
	gettimeofday(&now_time, NULL);

	struct timeval* until_timep = NULL;	
	int index = 0;
	for(index=0; index<=g_http_session_pos; index++)
	{
		SESSION_T* sessionp = &g_http_sessions[index];	
		if(sessionp->sessionp ==  NULL && sessionp->begin_time.tv_sec == 0)
		{
			continue;
		}

		if(sessionp->end_time.tv_sec == 0)
		{
			until_timep = &now_time;
		}
		else
		{
			until_timep = &sessionp->end_time;
		}

		u_int64_t upload_rate = network_rate(sessionp->upload_bytes, &sessionp->begin_time, until_timep);
		u_int64_t download_rate = network_rate(sessionp->download_bytes, &sessionp->begin_time, until_timep);

		char session_ip[MAX_IP_LEN] = {'\0'} ;
		struct in_addr s = {};
		s.s_addr = sessionp->remote_ip;
		inet_ntop(AF_INET, (const void *)&s, session_ip, MAX_IP_LEN);
		
		content.PutFmtStr("\t<session remote_ip=\"0x%08X[%s]\" remote_port=\"%d\" session_type=\"0x%08X\" "
			"upload_bytes=\"%ld\" download_bytes=\"%ld\" "
			"begin_time=\"%ld.%06ld\" end_time=\"%ld.%06ld\" now_time=\"%ld.%06ld\" "
			"upload_rate=\"%ld bps\" download_rate=\"%ld bps\">\n", 
			sessionp->remote_ip, session_ip, sessionp->remote_port, sessionp->session_type,
			sessionp->upload_bytes, sessionp->download_bytes,
			sessionp->begin_time.tv_sec, sessionp->begin_time.tv_usec, 
			sessionp->end_time.tv_sec,   sessionp->end_time.tv_usec,
			now_time.tv_sec, now_time.tv_usec,
			upload_rate, download_rate);		

		content.Put("\t</session>\n");
	}
	content.Put("</sessions>\n");
	
	//ret = ResponseContent(content.GetBufPtr(), content.GetBytesWritten(), CONTENT_TYPE_APPLICATION_XML);
	ret = ResponseHeader(content.GetBufPtr(), content.GetBytesWritten(), CONTENT_TYPE_APPLICATION_XML);

	fCmdContentLength = content.GetBytesWritten();	
	if(fRequest.fMethod == httpGetMethod)
	{
		fCmdContentPosition = 0;
	}
	else if(fRequest.fMethod == httpHeadMethod)
	{
		free(fCmdBuffer);
		fCmdBuffer = NULL;
		fCmdBufferSize = 0;
		fCmdContentLength = 0;
		fCmdContentPosition = 0;
	}	

	return ret;
}

QTSS_Error HTTPSession::ResponseCmdGetConfig()
{
	QTSS_Error ret = QTSS_NoErr;
	
	char	buffer[4*1024];
	StringFormatter content(buffer, sizeof(buffer));
	content.PutFmtStr(
		"ip=%s\n"		
		"port=%u\n"
		"service_ip=%s\n"
		"bin_path=%s\n"
		"etc_path=%s\n"
		"log_path=%s\n"
		"html_path=%s\n"
		"max_clip_num=%d\n"
		"clip_duration=%d s\n"
		"download_interval=%d ms\n"
		"download_limit=%ld bps\n", 
		g_config.ip,
		g_config.port,
		g_config.service_ip,
		g_config.bin_path,
		g_config.etc_path, 
		g_config.log_path, 
		g_config.html_path,
		g_config.max_clip_num - 1,
		g_config.clip_duration,
		g_config.download_interval,
		g_config.download_limit);	

	ResponseContent(content.GetBufPtr(), content.GetBytesWritten(), CONTENT_TYPE_TEXT_PLAIN);

	return ret;
}

QTSS_Error HTTPSession::ResponseCmdSetConfig()
{
	QTSS_Error ret = QTSS_NoErr;

	char* key1 	 = "download_interval";
	char* value1 = params_get_key(fRequest.fParamPairs, key1);
	char* key2 	 = "download_limit";
	char* value2 = params_get_key(fRequest.fParamPairs, key2);
	if(value1 == NULL && value2 == NULL)
	{
		char* request_file = "/set_config.html";
		char abs_path[PATH_MAX];
		snprintf(abs_path, PATH_MAX, "%s%s", g_config.html_path, request_file);
		abs_path[PATH_MAX-1] = '\0';	
		ret = ResponseFile(abs_path);		
		return ret;
	}

	if(value1 != NULL)
	{
		int download_interval = atoi(value1);
		if(download_interval < MIN_DOWNLOAD_INTERVAL)
		{
			char reason[MAX_REASON_LEN] = "";
			snprintf(reason, MAX_REASON_LEN, "download_interval[%d] < MIN_DOWNLOAD_INTERVAL[%d]", download_interval, MIN_DOWNLOAD_INTERVAL);
			reason[MAX_REASON_LEN-1] = '\0';
			ret = ResponseCmdResult(CMD_SET_CONFIG, "error", "failure", reason);
			return ret;
		}
		g_config.download_interval = download_interval; 
	}
	if(value2 != NULL)
	{
		long download_limit = atol(value2);
		if(download_limit>=0 && download_limit < MIN_DOWNLOAD_LIMIT)
		{
			char reason[MAX_REASON_LEN] = "";
			snprintf(reason, MAX_REASON_LEN, "download_limit[%ld] < MIN_DOWNLOAD_LIMIT[%ld]", download_limit, (long)MIN_DOWNLOAD_LIMIT);
			reason[MAX_REASON_LEN-1] = '\0';
			ret = ResponseCmdResult(CMD_SET_CONFIG, "error", "failure", reason);
			return ret;
		}
		g_config.download_limit = download_limit; 
	}
	
	int result = config_write(&g_config, g_config_file);
	if(result != 0)
	{
		ret = ResponseCmdResult(CMD_SET_CONFIG, "error", "failure", "config_write() internal error");
		return ret;
	}
	
	ret = ResponseCmdResult(CMD_SET_CONFIG, "ok", "success", "");	
	return ret;

	
}

QTSS_Error HTTPSession::ResponseCmdResult(char* cmd, char* return_val, char* result, char* reason)
{
	char	buffer[4*1024];
	StringFormatter content(buffer, sizeof(buffer));

	if(fCmd.format == 0)
	{
		content.PutFmtStr("return=%s\r\n", return_val);
		content.PutFmtStr("result=%s[%s]\r\n", result, reason);
		content.PutFmtStr("cmd=%s\r\n", cmd);
		content.PutFmtStr("server=%s/%s\r\n", BASE_SERVER_NAME, BASE_SERVER_VERSION);
		time_t now = time(NULL);
		char str_now[MAX_TIME_LEN] = {0};
		ctime_r(&now, str_now);
		str_now[strlen(str_now)-1] = '\0';	
		content.PutFmtStr("time=%s\r\n", str_now);
		
		ResponseContent(content.GetBufPtr(), content.GetBytesWritten(), CONTENT_TYPE_TEXT_PLAIN);
	}
	else
	{
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
		content.Put("return:\n");
		content.Put("</TH>\n");
		content.Put("<TD align=\"left\">\n");
		content.PutFmtStr("%s\n", return_val);
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
		char str_now[MAX_TIME_LEN] = {0};
		ctime_r(&now, str_now);
		content.PutFmtStr("%s", str_now);
		content.Put("</TD>\n");	
		content.Put("</TR>\n");

		content.Put("</TABLE>\n");		
		content.Put("</BODY>\n");
		content.Put("</HTML>\n");
		
		ResponseContent(content.GetBufPtr(), content.GetBytesWritten(), CONTENT_TYPE_TEXT_HTML);
	}
    
	return QTSS_NoErr;
}

Bool16 HTTPSession::ResponseContent(char* content, int len, char* type, Bool16 no_cache)
{	
	fHttpStatus = httpOK;
	fContentLen = len;
	
	fResponse.Set(fStrRemained.Ptr+fStrRemained.Len, kResponseBufferSizeInBytes-fStrRemained.Len);
	fResponse.PutFmtStr("%s %s %s\r\n", 
    	HTTPProtocol::GetVersionString(http11Version)->Ptr,
    	HTTPProtocol::GetStatusCodeAsString(httpOK)->Ptr,
    	HTTPProtocol::GetStatusCodeString(httpOK)->Ptr);
	fResponse.PutFmtStr("Server: %s/%s\r\n", BASE_SERVER_NAME, BASE_SERVER_VERSION);
	if(no_cache)
	{
		fResponse.Put("Cache-Control: no-cache\r\n");
	}
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

Bool16 HTTPSession::ResponseHeader(char* content, int len, char* type)
{		
	fHttpStatus = httpOK;
	fContentLen = len;
	
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

	memset(&fCmd, 0, sizeof(CMD_T));
	cmd_read_params(&fCmd, fRequest.fParamPairs);
	
	if(strcmp(fCmd.cmd, CMD_LIST_CHANNEL) == 0)
	{
		ret = ResponseCmdListChannel();
	}
	else if(strcmp(fCmd.cmd, CMD_ADD_CHANNEL) == 0)
	{		
		ret = ResponseCmdAddChannel();
	}
	else if(strcmp(fCmd.cmd, CMD_DEL_CHANNEL) == 0)
	{
		ret = ResponseCmdDelChannel();
	}
	else if(strcmp(fCmd.cmd, CMD_CHANNEL_STATUS) == 0)
	{
		ret = ResponseCmdChannelStatus();
	}
	else if(strcmp(fCmd.cmd, CMD_HTTP_SESSIONS) == 0)
	{
		ret = ResponseCmdHttpSessions();
	}
	else if(strcmp(fCmd.cmd, CMD_SESSION_LIST) == 0)
	{
		ret = ResponseCmdSessionList();
	}
	else if(strcmp(fCmd.cmd, CMD_QUERY_VERSION) == 0)
	{		
		ret = ResponseCmdResult(CMD_QUERY_VERSION, "ok", CMD_VERSION"|"MY_VERSION, "");
	}
	else if(strcmp(fCmd.cmd, CMD_QUERY_PROCESS) == 0)
	{		
		ret = ResponseCmdQueryProcess();
	}
	else if(strcmp(fCmd.cmd, CMD_QUERY_CHANNEL) == 0)
	{
		ret = ResponseCmdQueryChannel();
	}
	else if(strcmp(fCmd.cmd, CMD_QUERY_SESSION) == 0)
	{
		ret = ResponseCmdQuerySession();
	}
	else if(strcmp(fCmd.cmd, CMD_GET_CONFIG) == 0)
	{
		ret = ResponseCmdGetConfig();
	}
	else if(strcmp(fCmd.cmd, CMD_SET_CONFIG) == 0)
	{
		ret = ResponseCmdSetConfig();
	}
	else 
	{
		fHttpStatus = httpBadRequest;
		ret = ResponseError(fHttpStatus);
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
		fHttpStatus = httpNotFound;
		ret = ResponseError(fHttpStatus);
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
	//MEMORY_T* memoryp = NULL;	
	fMemory = NULL;	
	STATISTICS_T* statisticsp = NULL;
	if(strcasecmp(param_type, LIVE_TS) == 0)
	{
		fHttpClientSession = channelp->sessionp_ts;
		//memoryp = channelp->memoryp_ts;
		fMemory = channelp->memoryp_ts;
		statisticsp = &(channelp->statistics_ts);
	}
	else if(strcasecmp(param_type, LIVE_FLV) == 0)
	{
		fHttpClientSession = channelp->sessionp_flv;
		//memoryp = channelp->memoryp_flv;
		fMemory = channelp->memoryp_flv;
		statisticsp = &(channelp->statistics_flv);
	}
	else if(strcasecmp(param_type, LIVE_MP4) == 0)
	{
		fHttpClientSession = channelp->sessionp_mp4;
		//memoryp = channelp->memoryp_mp4;
		fMemory = channelp->memoryp_mp4;
		statisticsp = &(channelp->statistics_mp4);
	}
	//if(memoryp == NULL)
	if(fHttpClientSession == NULL)
	{
		ret = ResponseError(httpNotFound);
		return ret;
	}

	if(fStatistics != statisticsp)
	{
		if(fStatistics != NULL)
		{
			fStatistics->session_num --;
		}
		statisticsp->session_num ++;
		fStatistics = statisticsp;
	}

	fprintf(stdout, "%s: before fDefaultThread=0x%016lX, fUseThisThread=0x%016lX, %ld\n", 
		__PRETTY_FUNCTION__, (long)this->fDefaultThread, (long)this->fUseThisThread, pthread_self());
	TaskThread* threadp = fHttpClientSession->GetDefaultThread();
	if(threadp == NULL)
	{
		fHttpStatus = httpInternalServerError;
		ret = ResponseError(fHttpStatus);
		return ret;
	}
	
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
		fHttpStatus = httpGone;
		ret = ResponseError(fHttpStatus);
		return ret;
	}

	MEMORY_T* memoryp = fHttpClientSession->GetMemory();
	if(fLiveLen == -1)
	{
		fLiveLen = DEFAULT_SEGMENT_NUM;
	}
	
		
	if(memoryp->clip_num <= 0)
	{
		fHttpStatus = httpNotFound;
		ResponseError(fHttpStatus);
		return ret;
	}
		
	int index = memoryp->clip_index - 1;	
	if(index<0)
	{
		index = g_config.max_clip_num - 1;
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
								
		index --;
		if(index<0)
		{
			index = g_config.max_clip_num - 1;
		}

		count ++;
		if(count >= memoryp->clip_num || (fLiveLen !=-1 && count >= fLiveLen) )
		{
			break;
		}
	}

	index ++;
	if(index>=g_config.max_clip_num)
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
		count2 ++;
		if(count2>count)
		{
			break;
		}
		
		CLIP_T* onep = &(memoryp->clips[index]);
		content.PutFmtStr("#EXTINF:%u,\n", onep->inf);
		if(strcasecmp(fLiveType, LIVE_TS) == 0)
		{
			// do nothing.
		}
		else
		{
			content.PutFmtStr("#EXT-X-BYTERANGE:%lu\n", onep->byte_range);
		}
		#if 0
		content.PutFmtStr("%s\n", onep->m3u8_relative_url);
		#else
		content.PutFmtStr("http://%s:%u/%s/%s\n", g_config.service_ip, g_config.port, fHttpClientSession->GetM3U8Path(), onep->m3u8_relative_url);
		#endif
		
		index ++;
		if(index>=g_config.max_clip_num)
		{
			index = 0;
		}
		
		
	}
	//content.PutTerminator();

	ResponseContent(m3u8_buffer, content.GetBytesWritten(), CONTENT_TYPE_APPLICATION_M3U8, TRUE);	
	return ret;
}


QTSS_Error HTTPSession::ResponseLiveSegment()
{
	QTSS_Error ret = QTSS_NoErr;

	fLiveRequest = kLiveSegment;
	
	char* param_type = LIVE_FLV;
	int   param_seq = -1;
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
		
		if(nodep->nextp == fRequest.fParamPairs)
		{
			break;	
		}
		nodep = nodep->nextp;
	}
	
	fLiveSeq = param_seq;
	if(fLiveSeq != -1)
	{
		// /livestream/78267cf4a7864a887540cf4af3c432dca3d52050?seq=3059413
		StringParser parser(&(fRequest.fRelativeURI));
		parser.ConsumeLength(NULL, strlen(URI_LIVESTREAM));
		StrPtrLen seg1;
		parser.ConsumeUntil(&seg1, '?');
		parser.Expect('?');		
		int len = seg1.Len;
		if(len >= MAX_LIVE_ID-1)
		{
			len = MAX_LIVE_ID-1;
		}
		memcpy(fLiveId, seg1.Ptr, len);
		fLiveId[len] = '\0';
		
		strncpy(fLiveType, param_type, MAX_LIVE_TYPE-1);
		fLiveType[MAX_LIVE_TYPE-1] = '\0';		
	}
	else
	{	
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
	}
	
	CHANNEL_T* channelp = g_channels.FindChannelByHash(fLiveId);
	if(channelp == NULL)
	{
		ret = ResponseError(httpNotFound);
		return ret;
	}

	fHttpClientSession = NULL;
	//MEMORY_T* memoryp = NULL;
	fMemory = NULL;
	STATISTICS_T* statisticsp = NULL;
	char* mime_type = CONTENT_TYPE_APPLICATION_OCTET_STREAM;
	if(strcasecmp(fLiveType, LIVE_TS) == 0)
	{
		fHttpClientSession = channelp->sessionp_ts;
		//memoryp = channelp->memoryp_ts;
		fMemory = channelp->memoryp_ts;
		statisticsp = &(channelp->statistics_ts);
		mime_type = CONTENT_TYPE_VIDEO_MP2T;
	}
	else if(strcasecmp(fLiveType, LIVE_FLV) == 0)
	{
		fHttpClientSession = channelp->sessionp_flv;
		//memoryp = channelp->memoryp_flv;
		fMemory = channelp->memoryp_flv;
		statisticsp = &(channelp->statistics_flv);
		mime_type = CONTENT_TYPE_VIDEO_FLV;
	}
	else if(strcasecmp(fLiveType, LIVE_MP4) == 0)
	{
		fHttpClientSession = channelp->sessionp_mp4;
		//memoryp = channelp->memoryp_mp4;
		fMemory = channelp->memoryp_mp4;
		statisticsp = &(channelp->statistics_mp4);
		mime_type = CONTENT_TYPE_VIDEO_MP4;
	}
	//if(memoryp == NULL)
	if(fHttpClientSession == NULL)
	{
		ret = ResponseError(httpNotFound);
		return ret;
	}

	if(fStatistics != statisticsp)
	{
		if(fStatistics != NULL)
		{
			fStatistics->session_num --;
		}
		statisticsp->session_num ++;
		fStatistics = statisticsp;
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
		fHttpStatus = httpGone;
		ret = ResponseError(fHttpStatus);
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
		index = g_config.max_clip_num - 1;
	}
		
	int count = 0;
	while(1)
	{
		count ++;
		if(count > memoryp->clip_num)
		{
			break;
		}
		
		CLIP_T* onep = &(memoryp->clips[index]);
		if((fLiveSeq!= -1 && (int64_t)onep->sequence == fLiveSeq) || 
			(strncmp(onep->relative_url, fRequest.fRelativeURI.Ptr, strlen(onep->relative_url)) == 0) )
		{
			clipp = onep;
			break;
		}
		
		index --;
		if(index<0)
		{
			index = g_config.max_clip_num - 1;
		}
	}
	if(clipp == NULL)
	{
		fHttpStatus = httpNotFound;
		ret = ResponseError(fHttpStatus);
		return ret;
	}

	fData = &(clipp->data);
	fDataPosition = fRangeStart;
	if(fRangeStop == -1)
	{
		fRangeStop = fData->len-1;
	}

	fHttpStatus = httpOK;	
	if(fHaveRange)
	{
		if(fRangeStart > fRangeStop)
		{
			fHttpStatus = httpRequestRangeNotSatisfiable;
			ret = ResponseError(fHttpStatus);
			fData = NULL;
			return ret;
		}
		if(fRangeStop > fData->len-1)
		{
			fHttpStatus = httpRequestRangeNotSatisfiable;
			ret = ResponseError(fHttpStatus);
			fData = NULL;
			return ret;
		}
		
		fprintf(stdout, "%s: range=%ld-%ld", __PRETTY_FUNCTION__, fRangeStart, fRangeStop);
		fHttpStatus = httpPartialContent;
	}

	fContentLen = fRangeStop+1-fRangeStart;
	
	fResponse.Set(fStrRemained.Ptr+fStrRemained.Len, kResponseBufferSizeInBytes-fStrRemained.Len);
	fResponse.PutFmtStr("%s %s %s\r\n", 
			HTTPProtocol::GetVersionString(http11Version)->Ptr,
			HTTPProtocol::GetStatusCodeAsString(fHttpStatus)->Ptr,
			HTTPProtocol::GetStatusCodeString(fHttpStatus)->Ptr);
    fResponse.PutFmtStr("Server: %s/%s\r\n", BASE_SERVER_NAME, BASE_SERVER_VERSION);
    fResponse.PutFmtStr("Accept-Ranges: bytes\r\n");	
    fResponse.PutFmtStr("Content-Length: %ld\r\n", fContentLen); 
    fResponse.PutFmtStr("Connection: keep-alive\r\n");
    fResponse.PutFmtStr("Proxy-Connection: Keep-Alive\r\n");
    fResponse.PutFmtStr("Content-Disposition: attachment;filename=\"%s\"\r\n", clipp->file_name); 
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
	int ret = QTSS_NoErr;
	
	fFd = open(abs_path, O_RDONLY);
	if(fFd == -1)
	{
		fHttpStatus = httpInternalServerError;
		ret = ResponseError(fHttpStatus);
		return ret;
	}	
	
	off_t file_len = lseek(fFd, 0L, SEEK_END);	
	if(fRangeStop == -1)
	{
		fRangeStop = file_len - 1;
	}	

	fHttpStatus = httpOK;	
	if(fHaveRange)
	{
		if(fRangeStart > fRangeStop)
		{
			fHttpStatus = httpRequestRangeNotSatisfiable;
			ret = ResponseError(fHttpStatus);
			close(fFd);
			fFd = -1;
			return ret;
		}
		if(fRangeStop > file_len - 1)
		{
			fHttpStatus = httpRequestRangeNotSatisfiable;
			ret = ResponseError(fHttpStatus);
			close(fFd);
			fFd = -1;
			return ret;
		}
		
		fprintf(stdout, "%s: range=%ld-%ld", __PRETTY_FUNCTION__, fRangeStart, fRangeStop);
		fHttpStatus = httpPartialContent;
	}
	
	lseek(fFd, fRangeStart, SEEK_SET);
	
	char* suffix = file_suffix(abs_path);
	char* content_type = content_type_by_suffix(suffix);
	fContentLen = fRangeStop+1-fRangeStart;
	
	fResponse.Set(fStrRemained.Ptr+fStrRemained.Len, kResponseBufferSizeInBytes-fStrRemained.Len);		
	fResponse.PutFmtStr("%s %s %s\r\n", 
			HTTPProtocol::GetVersionString(http11Version)->Ptr,
			HTTPProtocol::GetStatusCodeAsString(fHttpStatus)->Ptr,			
			HTTPProtocol::GetStatusCodeString(fHttpStatus)->Ptr);
    fResponse.PutFmtStr("Server: %s/%s\r\n", BASE_SERVER_NAME, BASE_SERVER_VERSION);
    fResponse.PutFmtStr("Accept-Ranges: bytes\r\n");	
    fResponse.PutFmtStr("Content-Length: %ld\r\n", fContentLen);    
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
	char str_now[MAX_TIME_LEN] = {0};
	ctime_r(&now, str_now);
	content.PutFmtStr("%s", str_now);
	content.Put("</TD>\n");	
	content.Put("</TR>\n");

	content.Put("</TABLE>\n");		
	content.Put("</BODY>\n");
	content.Put("</HTML>\n");

    fContentLen = content.GetBytesWritten();
    
    fResponse.Set(fStrRemained.Ptr+fStrRemained.Len, kResponseBufferSizeInBytes-fStrRemained.Len);
    fResponse.PutFmtStr("%s %s %s\r\n", 
    	HTTPProtocol::GetVersionString(http11Version)->Ptr,
    	HTTPProtocol::GetStatusCodeAsString(status_code)->Ptr,
    	HTTPProtocol::GetStatusCodeString(status_code)->Ptr);
    fResponse.PutFmtStr("Server: %s/%s\r\n", BASE_SERVER_NAME, BASE_SERVER_VERSION);
    fResponse.PutFmtStr("Content-Length: %d\r\n", fContentLen);
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


