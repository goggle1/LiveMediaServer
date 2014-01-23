
#include <sys/stat.h>
#include <sys/types.h>
//#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "BaseServer/StringParser.h"
#include "BaseServer/StringFormatter.h"

#include "common.h"
#include "config.h"
#include "HTTPClientSession.h"

// guoqiang errorcodes
#define ENOT200 	2001
#define ETIMEOUT 	2002

int make_dir(StrPtrLen& dir)
{
	char path[PATH_MAX] = {'\0'};
	sprintf(path, "%s/", g_config.html_path);
	
	int path_len = strlen(path);	
	if(path_len+dir.Len >= PATH_MAX)
	{
		fprintf(stderr, "%s: dir is too long[%d][%s]\n", __FUNCTION__, dir.Len, dir.Ptr);
		return -1;
	}
	strncpy(path+path_len, dir.Ptr, dir.Len);
	path_len = path_len + dir.Len;
	path[path_len] = '\0';

	if(access(path, F_OK) == 0)
	{
		return 0;
	}
	
	int ret = mkdir(path, 0755);
	if(ret != 0)
	{
		fprintf(stderr, "%s: mkdir return %d, errno=[%d][%s]\n", __FUNCTION__, ret, errno, strerror(errno));
		return -1;
	}
	
	return 0;
}

HTTPClientSession::HTTPClientSession(CHANNEL_T* channelp, char* live_type)
	:fTimeoutTask(this, g_config.download_interval)
{		
	fprintf(stdout, "%s: live_type=%s, liveid=%s\n", __PRETTY_FUNCTION__, live_type, channelp->liveid);
	
	fSocket = new TCPClientSocket(Socket::kNonBlockingSocketType);	
	
	fClient = new HTTPClient(fSocket);	

	fChannel= channelp;
	
	fSourceNum = 0;
	fSourceList	= NULL;
	fSourceNow = NULL;
	fWillSourceList = NULL;
	
	strncpy(fLiveType, live_type, MAX_LIVE_TYPE-1);
	fLiveType[MAX_LIVE_TYPE-1] = '\0';
	strcpy(fM3U8Path, "livestream");
	StrPtrLen path(fM3U8Path);
	fM3U8Parser.SetPath(&path);

	char task_name[32] = {'\0'};
	snprintf(task_name, 32, "c_%s_%c%c%c%c", fLiveType, 
		channelp->liveid[0], channelp->liveid[1], channelp->liveid[2], channelp->liveid[3]);
	task_name[32 - 1] = '\0';
	this->SetTaskName(task_name);

	fLastChunkId = 0;
	fWithChunkId = false;		

	fState = kSendingGetM3U8;	
	fGetIndex	= 0;
	fWillStop = false;
	fWillUpdateSources = false;

	gettimeofday(&fBeginTime, &fTimeZone);
	fLogTime.tv_sec = 0;
	fLogTime.tv_usec = 0;
#if 0
	fLogTime = fBeginTime;
	snprintf(fLogFile, PATH_MAX, "%s/%d_%d_%s_%s_%ld_%06ld.log", 
		g_config.work_path, getpid(), gettid(), fLiveType, fChannel->liveid,
		fLogTime.tv_sec, fLogTime.tv_usec);
	fLogFile[PATH_MAX-1] = '\0';
	fLog = fopen(fLogFile, "a");
#endif
	
	fMemory = (MEMORY_T*)malloc(sizeof(MEMORY_T));	
	// if fMemory == NULL, thrown exception.
	memset(fMemory, 0, sizeof(MEMORY_T));
	fMemory->clips = (CLIP_T*)malloc(g_config.max_clip_num * sizeof(CLIP_T));
	memset(fMemory->clips, 0, g_config.max_clip_num * sizeof(CLIP_T));
	if(strcasecmp(fLiveType, LIVE_TS) == 0)
	{
		channelp->memoryp_ts = fMemory;
		channelp->sessionp_ts = this;
	}
	else if(strcasecmp(fLiveType, LIVE_FLV) == 0)
	{
		channelp->memoryp_flv = fMemory;
		channelp->sessionp_flv = this;
	}
	else if(strcasecmp(fLiveType, LIVE_MP4) == 0)
	{
		channelp->memoryp_mp4 = fMemory;
		channelp->sessionp_mp4 = this;
	}

	fLog = NULL;		
	//this->Signal(Task::kStartEvent);

	
}

HTTPClientSession::~HTTPClientSession()
{	
	gettimeofday(&fEndTime, NULL);	
	if(fLog != NULL)
	{
		fprintf(fLog, "%s, end: %ld.%06ld\n", __PRETTY_FUNCTION__, fEndTime.tv_sec, fEndTime.tv_usec);
		fclose(fLog);
		fLog = NULL;
	}

	if(fMemory != NULL)
	{
		int index = 0;
		for(index=0; index<MAX_M3U8_NUM; index++)
		{
			if(fMemory->m3u8s[index].data.datap != NULL)
			{
				fMemory->m3u8s[index].data.len = 0;
				fMemory->m3u8s[index].data.size = 0;
				free(fMemory->m3u8s[index].data.datap);				
			}
		}
		for(index=0; index<g_config.max_clip_num; index++)
		{
			if(fMemory->clips[index].data.datap != NULL)
			{
				fMemory->clips[index].data.len = 0;
				fMemory->clips[index].data.size = 0;
				free(fMemory->clips[index].data.datap);				
			}
		}
		
		free(fMemory);
		fMemory = NULL;
	}
	
	
	if(fClient != NULL)
	{
		delete fClient;
		fClient = NULL;
	}
	if(fSocket != NULL)
	{
		delete fSocket;
		fSocket = NULL;
	}
	fprintf(stdout, "%s\n", __PRETTY_FUNCTION__);
}

int HTTPClientSession::MakeUrlM3U8()
{
	if(fWithChunkId)
	{
		snprintf(fUrlM3U8, MAX_URL_LEN, "/%s/%s.m3u8?codec=%s&len=%d&seq=%lu", fM3U8Path, fChannel->liveid, fLiveType, MAX_SEGMENT_NUM, fLastChunkId);
		fUrlM3U8[MAX_URL_LEN-1] = '\0';
	}
	else
	{
		snprintf(fUrlM3U8, MAX_URL_LEN, "/%s/%s.m3u8?codec=%s&len=%d", fM3U8Path, fChannel->liveid, fLiveType, MAX_SEGMENT_NUM);
		fUrlM3U8[MAX_URL_LEN-1] = '\0';	
	}
	return 0;
}

int HTTPClientSession::Start()
{
	this->Signal(Task::kStartEvent);
	return 0;
}

int HTTPClientSession::Stop()
{
	fWillStop = true;
	return 0;
}

int HTTPClientSession::TryStop()
{
	if(!fWillStop)
	{
		return -1;
	}
	
	fprintf(stdout, "%s\n", __PRETTY_FUNCTION__);
	fState = kDone;
	this->Signal(Task::kKillEvent);
	fWillStop = false;
	return 0;
	
}


int HTTPClientSession::SetSources(DEQUE_NODE* source_list)
{	
	int ret = 0;

	fprintf(stdout, "%s\n", __PRETTY_FUNCTION__);
	
	fSourceList = source_list;
	fSourceNum = deque_num(fSourceList);
	if(fSourceNum == 0)
	{
		return -1;
	}
	
	int SourceIndex = rand() % fSourceNum;
	fSourceNow = deque_index(fSourceList, SourceIndex);
	SOURCE_T* sourcep = (SOURCE_T*)fSourceNow->datap;	
	ret = SetSource(sourcep);
	
	return ret;
}

int HTTPClientSession::UpdateSources(DEQUE_NODE* source_list)
{
	fWillUpdateSources = true;
	fWillSourceList = source_list;
	return 0;
}

int HTTPClientSession::TrySwitchSources()
{	
	int ret = 0;

	if(!fWillUpdateSources)
	{
		return -1;
	}
		
	fprintf(stdout, "%s\n", __PRETTY_FUNCTION__);
	
	if(fSourceNow != NULL)
	{
		fSourceNum = 0;
		fSourceList	= NULL;
		fSourceNow = NULL;
		fClient->Disconnect();
	}
	
	fState = kSendingGetM3U8;	

	ret = SetSources(fWillSourceList);	
	fWillSourceList = NULL;
	fWillUpdateSources = false;
	
	return ret;
}

int HTTPClientSession::SwitchLog(struct timeval until)
{
	time_t	second1 = fLogTime.tv_sec;
	time_t	second2 = until.tv_sec;
	time_t day1 = (second1 - fTimeZone.tz_minuteswest*60) / (3600*24);
	time_t day2 = (second2 - fTimeZone.tz_minuteswest*60) / (3600*24);
	if(day1 == day2)
	{
		return -1;
	}
	
	if(fLog == NULL)
	{
		fLogTime = until;
	}
	else
	{
		fLogTime.tv_sec = day2*3600*24 + fTimeZone.tz_minuteswest*60;
		fLogTime.tv_usec = 0;
		fclose(fLog);
		fLog = NULL;
	}

	//fLogTime = until;
	snprintf(fLogFile, PATH_MAX, "%s/%d_%d_%s_%s_%ld_%06ld.log", 
		g_config.log_path, getpid(), gettid(), fLiveType, fChannel->liveid,
		fLogTime.tv_sec, fLogTime.tv_usec);
	fLogFile[PATH_MAX-1] = '\0';
	fLog = fopen(fLogFile, "a");
	return 0;
}

int HTTPClientSession::SwitchSource(OS_Error theErr)
{
	int ret = 0;
	
	//ENOT200
	//ETIMEOUT
	// else
	// network errno
	
	fprintf(stdout, "%s: LiveType=%s, LiveId=%s\n", __PRETTY_FUNCTION__, fLiveType, fChannel->liveid);	
	if(fSourceNum <= 1)
	{
		// network errno , or timeout
		//if(theErr != ENOT200 && theErr != ETIMEOUT)
		if(theErr != ENOT200)
		{
			// disconnect
			fClient->Disconnect();
		}
		return -1;
	}
	
	//fState = kSendingGetM3U8;	
	ret = fClient->Disconnect();
	fSourceNow = fSourceNow->nextp;
	SOURCE_T* sourcep = (SOURCE_T*)fSourceNow->datap;		
	ret = SetSource(sourcep);
	
	return ret;
}

int HTTPClientSession::SetSource(SOURCE_T* sourcep)
{
	int ret = 0;
	
	UInt32	ip_net = htonl(sourcep->ip);
	struct in_addr in;
	in.s_addr = ip_net;
	char* 	ip_str = inet_ntoa(in);	
	snprintf(fHost, MAX_HOST_LEN, "%s:%d", ip_str, sourcep->port);
	fHost[MAX_HOST_LEN-1] = '\0';

	fprintf(stdout, "%s: LiveType=%s, LiveId=%s, Source: %s\n", __PRETTY_FUNCTION__, fLiveType, fChannel->liveid, fHost);
	if(fLog != NULL)
	{
   		fprintf(fLog, "SetSource: %s\n", fHost);
	}
    	
	ret = fClient->SetSource(sourcep->ip, sourcep->port);

	return ret;
}

Bool16 HTTPClientSession::ConnectionTimeout()
{
	// tcp connect timeout
	if(fClient->ConnectTimeout())
	{
		fprintf(stdout, "%s: %s, source: %s, error: %s, begin: %ld.%06ld, end: %ld.%06ld\n",  
			__PRETTY_FUNCTION__, 
			fClient->fUrl, fClient->fHost, "connection_timeout", 
			fClient->fBeginTime.tv_sec, fClient->fBeginTime.tv_usec,
			fClient->fEndTime.tv_sec,   fClient->fEndTime.tv_usec);	
		if(fLog != NULL)
    	{
    		char str_begin_time[MAX_TIME_LEN] = {0};
    		char str_end_time[MAX_TIME_LEN] = {0};
			ctime_r(&fClient->fBeginTime.tv_sec, str_begin_time);
			str_begin_time[strlen(str_begin_time)-1] = '\0';
			ctime_r(&fClient->fEndTime.tv_sec, str_end_time);
			str_end_time[strlen(str_end_time)-1] = '\0';
       		fprintf(fLog, "%s, source: %s, error: %s, begin: %ld.%06ld [%s], end: %ld.%06ld [%s]\n", 
        		fClient->fUrl, fClient->fHost, "connection_timeout", 
        		fClient->fBeginTime.tv_sec, fClient->fBeginTime.tv_usec,	str_begin_time,
        		fClient->fEndTime.tv_sec,   fClient->fEndTime.tv_usec, 	str_end_time);
    	}
		return true;
	}

	return false;

}

Bool16 HTTPClientSession::DownloadTimeout()
{
	// download timeout
	int ret = timeval_cmp(&fClient->fEndTime, &fClient->fBeginTime);
	if(ret < 0)
	{
		return false;
	}
	else if(ret == 0)
	{
		return false;
	}
	
	time_t diff_time = timeval_diff(&fClient->fEndTime, &fClient->fBeginTime);
	if(diff_time > MAX_DOWNLOAD_TIME)
	{
		fprintf(stdout, "%s: %s, source: %s, error: %s[%u:%u:%u], begin: %ld.%06ld, end: %ld.%06ld\n",  
			__PRETTY_FUNCTION__, 
			fClient->fUrl, fClient->fHost, "download_timeout", fClient->fRangeStart, fClient->fContentRecvLen, fClient->fContentLength,
			fClient->fBeginTime.tv_sec, fClient->fBeginTime.tv_usec,
			fClient->fEndTime.tv_sec,   fClient->fEndTime.tv_usec);	
		if(fLog != NULL)
    	{
    		char str_begin_time[MAX_TIME_LEN] = {0};
    		char str_end_time[MAX_TIME_LEN] = {0};
			ctime_r(&fClient->fBeginTime.tv_sec, str_begin_time);
			str_begin_time[strlen(str_begin_time)-1] = '\0';
			ctime_r(&fClient->fEndTime.tv_sec, str_end_time);
			str_end_time[strlen(str_end_time)-1] = '\0';
       		fprintf(fLog, "%s, source: %s, error: %s[%u:%u:%u], begin: %ld.%06ld [%s], end: %ld.%06ld [%s]\n", 
        		fClient->fUrl, fClient->fHost, "download_timeout", fClient->fRangeStart, fClient->fContentRecvLen, fClient->fContentLength,
        		fClient->fBeginTime.tv_sec, fClient->fBeginTime.tv_usec,	str_begin_time,
        		fClient->fEndTime.tv_sec,   fClient->fEndTime.tv_usec, 	str_end_time);
    	}
		return true;
	}
	
	return false;
}

#if 1
Bool16 HTTPClientSession::IsDownloaded(SEGMENT_T * segp)
{
	if(segp->sequence <= fLastChunkId)
	{
		return true;
	}
	return false;
}

#else
Bool16 HTTPClientSession::IsDownloaded(SEGMENT_T * segp)
{	
	CLIP_T* clipp = NULL;	
	int index = fMemory->clip_index - 1;	
	if(index<0)
	{
		index = g_config.max_clip_num - 1;
	}
		
	int count = 0;
	while(1)
	{
		count ++;
		if(count > fMemory->clip_num)
		{
			break;
		}
		
		CLIP_T* onep = &(fMemory->clips[index]);
		if(strcmp(onep->relative_url, segp->relative_url) == 0)
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
		return false;
	}

	return true;
}
#endif

int HTTPClientSession::Log(char * url,char * datap, UInt32 len)
{
	StrPtrLen AbsoluteURI(url);
	StringParser urlParser(&AbsoluteURI);
  
    // we always should have a slash before the URI
    // If not, that indicates this is a full URI
    if (AbsoluteURI.Ptr[0] != '/')
    {
            //if it is a full URL, store the scheme and host name
            urlParser.ConsumeLength(NULL, 7); //consume "http://"
            urlParser.ConsumeUntil(NULL, '/');
    }    
	urlParser.Expect('/');
	
	StrPtrLen path;
	path.Ptr = urlParser.GetCurrentPosition();
	while(urlParser.GetDataRemaining() > 0)
	{
		StrPtrLen seg;
		urlParser.ConsumeUntil(&seg, '/');
		urlParser.Expect('/');
		if(seg.Ptr[seg.Len] == '/')
		{
			path.Len = urlParser.GetCurrentPosition()-path.Ptr;
			make_dir(path);
		}		
	}
	
	path.Len = urlParser.GetCurrentPosition()-path.Ptr;
	int ret = Write(path, datap, len);
    
	return ret;
}

int HTTPClientSession::RateSourceM3U8(time_t newest_time, u_int64_t download_bytes, struct timeval download_begin_time, struct timeval download_end_time)
{	
	if(fSourceNow == NULL)
	{
		return -1;
	}

	SOURCE_T* sourcep = (SOURCE_T*)fSourceNow->datap;		
	sourcep->download_rate.m3u8_newest_time = newest_time;
	sourcep->download_rate.m3u8_begin_time 	= download_begin_time;
	sourcep->download_rate.m3u8_end_time 	= download_end_time;
	sourcep->download_rate.m3u8_bytes		= download_bytes;
	
	return 0;
}

int HTTPClientSession::RateSourceSegment(u_int64_t download_bytes, struct timeval download_begin_time, struct timeval download_end_time)
{
	if(fSourceNow == NULL)
	{
		return -1;
	}

	SOURCE_T* sourcep = (SOURCE_T*)fSourceNow->datap;
	sourcep->download_rate.segment_begin_time 	= download_begin_time;
	sourcep->download_rate.segment_end_time		= download_end_time;
	sourcep->download_rate.segment_bytes		= download_bytes;
	
	return 0;
}

int HTTPClientSession::MoveSegment()
{
	fMemory->clip_index ++;
	if(fMemory->clip_index >= g_config.max_clip_num)
	{
		fMemory->clip_index = 0;
	}
	
	fMemory->clip_num ++;
	if(fMemory->clip_num > g_config.max_clip_num-1)
	{
		fMemory->clip_num = g_config.max_clip_num-1;
	}	

	return 0;
}

int HTTPClientSession::MemoSegment(SEGMENT_T* onep, UInt64 range_start, UInt64 content_length, char * datap, UInt32 len, time_t begin_time, time_t end_time)
{	
	CLIP_T* clipp = &(fMemory->clips[fMemory->clip_index]);	
	if(range_start == 0)
	{
		clipp->inf = onep->inf;
		clipp->byte_range = onep->byte_range;
		clipp->sequence = onep->sequence;	
		
		strncpy(clipp->relative_url, onep->relative_url, MAX_URL_LEN-1);
		clipp->relative_url[MAX_URL_LEN-1] = '\0';	

		strncpy(clipp->m3u8_relative_url, onep->m3u8_relative_url, MAX_URL_LEN-1);
		clipp->m3u8_relative_url[MAX_URL_LEN-1] = '\0';

		strncpy(clipp->file_name, onep->file_name, MAX_URL_LEN-1);
		clipp->file_name[MAX_URL_LEN-1] = '\0';

		clipp->data.len = 0;
	}

	fprintf(stdout, "%s: %s, range_start=%lu, content_length=%lu, len=%u\n", 
		__PRETTY_FUNCTION__, 
		clipp->relative_url,
		range_start, content_length, len);
	
	if(clipp->data.size >= content_length)
	{
		// do nothing.
	}
	else
	{
		void* newp = realloc(clipp->data.datap, content_length);
		if(newp == NULL)
		{
			return -1;
		}
		clipp->data.datap = newp;
		clipp->data.size = content_length;
	}
		

	#if 0
	if(range_start + len > content_length)
	{
		len = content_length - range_start;
	}
	#endif
	
	char* targetp = (char*)(clipp->data.datap) + range_start;
	memcpy(targetp, datap, len);
	clipp->data.len += len;

	if(range_start == 0)
	{
		clipp->begin_time = begin_time;
		clipp->end_time = 0;
	}

	if(range_start + len >= content_length)
	{
		clipp->end_time = end_time;
		
		MoveSegment();
	}
    
	return 0;
}


int HTTPClientSession::MemoM3U8(M3U8Parser* parserp, time_t begin_time, time_t end_time)
{
	fMemory->target_duration = parserp->fTargetDuration;
	
	M3U8_T* m3u8p = &(fMemory->m3u8s[fMemory->m3u8_index]);
	fMemory->m3u8_index ++;
	if(fMemory->m3u8_index >= MAX_M3U8_NUM)
	{
		fMemory->m3u8_index = 0;
	}
	

	if(m3u8p->data.datap == NULL)
	{	
		m3u8p->data.datap = malloc(MAX_M3U8_CONTENT_LEN);
		m3u8p->data.size = MAX_M3U8_CONTENT_LEN;		
	}
		
	if(m3u8p->data.datap != NULL)
	{
		StringFormatter content((char*)m3u8p->data.datap, m3u8p->data.size);	
		content.Put("#EXTM3U\n");
		content.PutFmtStr("#EXT-X-TARGETDURATION:%d\n", parserp->fTargetDuration);
		content.PutFmtStr("#EXT-X-MEDIA-SEQUENCE:%lu\n", parserp->fMediaSequence);
		int index = 0;
		for(index=0; index<parserp->fSegmentsNum; index++)
		{
			SEGMENT_T* segp = &(parserp->fSegments[index]);
			content.PutFmtStr("#EXTINF:%u,\n", segp->inf);
			if(strcasecmp(fLiveType, LIVE_TS) == 0)
			{
				// do nothing.
			}
			else
			{
				content.PutFmtStr("#EXT-X-BYTERANGE:%lu\n", segp->byte_range);
			}
			#if 0
			content.PutFmtStr("%s\n", segp->m3u8_relative_url);
			#else
			content.PutFmtStr("http://%s:%u/%s/%s\n", g_config.service_ip, g_config.port, fM3U8Path, segp->m3u8_relative_url);
			#endif
		}
	
		//content.PutTerminator();
		m3u8p->data.len = content.GetBytesWritten();
	}

	m3u8p->begin_time = begin_time;
	m3u8p->end_time = end_time;
	
	fMemory->m3u8_num ++;
	if(fMemory->m3u8_num > MAX_M3U8_NUM-1)
	{
		fMemory->m3u8_num = MAX_M3U8_NUM-1;
	}
	
	return 0;
}

int HTTPClientSession::RewriteM3U8(M3U8Parser* parserp)
{
	char path[PATH_MAX] = {'\0'};
	sprintf(path, "%s/%s_%s.m3u8", g_config.html_path, fLiveType, fChannel->liveid);
	int fd = open(path, O_WRONLY|O_CREAT|O_TRUNC, 0666);
	if(fd == -1)
	{
		fprintf(stderr, "%s: open return %d, errno=[%d][%s]\n", __FUNCTION__, fd, errno, strerror(errno));
		return -1;
	}
	
	int ret = 0;
	/*
	#EXTM3U
	#EXT-X-TARGETDURATION:10
	#EXT-X-MEDIA-SEQUENCE:2292
	#EXTINF:10,
	#EXT-X-PROGRAM-DATE-TIME:2013-10-28T15:34:11Z
	http://192.168.8.197:1180/41111/jiangsu/2013/10/28/41111-jiangsu-20131028-153411-2292.ts
	*/
	ret = dprintf(fd, "#EXTM3U\n");	
	ret = dprintf(fd, "#EXT-X-TARGETDURATION:%d\n", parserp->fTargetDuration);
	ret = dprintf(fd, "#EXT-X-MEDIA-SEQUENCE:%lu\n", parserp->fMediaSequence);
	
	int index = 0;
	for(index=0; index<parserp->fSegmentsNum; index++)
	{
		SEGMENT_T* segp = &(parserp->fSegments[index]);
		ret = dprintf(fd, "#EXTINF:%u,\n", segp->inf);
		if(strcasecmp(fLiveType, "ts") == 0)
		{
			// do nothing.
		}
		else
		{
			ret = dprintf(fd, "#EXT-X-BYTERANGE:%lu\n", segp->byte_range);
		}		
		ret = dprintf(fd, "%s\n", segp->relative_url);
	}

	close(fd);
	
	return 0;
}

int HTTPClientSession::Write(StrPtrLen& file_name, char * datap, UInt32 len)
{
	char path[PATH_MAX] = {'\0'};
	sprintf(path, "%s/", g_config.html_path);
	
	int path_len = strlen(path);	
	if(path_len+file_name.Len >= PATH_MAX)
	{
		fprintf(stderr, "%s: file_name is too long[%d][%s]\n", __FUNCTION__, file_name.Len, file_name.Ptr);
		return -1;
	}
	strncpy(path+path_len, file_name.Ptr, file_name.Len);
	path_len = path_len + file_name.Len;
	path[path_len] = '\0';

	int fd = open(path, O_WRONLY|O_CREAT|O_TRUNC, 0666);
	if(fd == -1)
	{
		fprintf(stderr, "%s: open return %d, errno=[%d][%s]\n", __FUNCTION__, fd, errno, strerror(errno));
		return -1;
	}

	ssize_t ret = write(fd, datap, len);
	close(fd);

	if(ret != len)
	{
		fprintf(stderr, "%s: open return %lu, errno=[%d][%s]\n", __FUNCTION__, ret, errno, strerror(errno));
		return -1;
	}

	return 0;
}

time_t HTTPClientSession::CalcBreakTime()
{
	struct timeval now;
	gettimeofday(&now, NULL);
	time_t diff_time = timeval_diff(&now, &fM3U8BeginTime);
	time_t break_time = g_config.download_interval;			            		
	if(diff_time >= g_config.download_interval)
	{
		break_time = 10;
	}
	else if(diff_time < 0)	// ajust time by admin.
	{
		break_time = 10;
	}
	else
	{
		break_time = g_config.download_interval - diff_time;
	}
	fprintf(stdout, "%s: diff_time=%ld, break_time=%ld\n", 
		__PRETTY_FUNCTION__, diff_time, break_time);
	return break_time;
}


int HTTPClientSession::GetSegmentInit()
{
	if(fGetSegment.get_index == fGetIndex)
	{
		return 0;
	}

	fGetSegment.get_index = fGetIndex;
	fGetSegment.range_start = 0;
	fGetSegment.download_count = 0;
	return 0;	
}

SInt64 HTTPClientSession::Run()
{	
	Task::EventFlags theEvents = this->GetEvents(); 
	//fprintf(stdout, "%s: theEvents=0x%08X\n", __PRETTY_FUNCTION__, theEvents);
	
	if (theEvents & Task::kStartEvent)
    {
		SwitchLog(fBeginTime);		
		SetSources(fChannel->source_list);
    }

	// We have been told to delete ourselves. Do so... NOW!!!!!!!!!!!!!!!
    if (theEvents & Task::kKillEvent)
    {
        return -1;
    }
    	
	if (theEvents & Task::kTimeoutEvent)
	{
		// do nothing.
		//fprintf(stdout, "%s: theEvents=0x%08X, kTimeoutEvent\n", __PRETTY_FUNCTION__, theEvents); 		
	}

    // Refresh the timeout. There is some legit activity going on...
    fTimeoutTask.RefreshTimeout();
        
	int ret = 0;
    OS_Error theErr = OS_NoErr;    
    while ((theErr == OS_NoErr) && (fState != kDone))
    {
    	ret = TryStop();
       	if(ret == 0)
       	{
       		//return 0;
       		return -1;
       	}
       	ret = TrySwitchSources();       	
               	
        //
        // Do the appropriate thing depending on our current state
        switch (fState)
        {
            case kSendingGetM3U8:
            {            	
            	fGetIndex = 0;
            	fGetSegment.get_index  = -1;
            	MakeUrlM3U8();
            	//fprintf(stdout, "%s: get %s\n", __PRETTY_FUNCTION__, fUrlM3U8);             	
            	theErr = fClient->SendGetM3U8(fUrlM3U8); 
            	fM3U8BeginTime = fClient->fBeginTime;
               	fM3U8EndTime = fClient->fEndTime;               	
            	if (theErr == OS_NoErr)
                {    
                	SwitchLog(fM3U8BeginTime);
                	UInt32 get_status = fClient->GetStatus();
                    if (get_status != 200)
                    {
                    	fprintf(stdout, "%s[0x%016lX]: %s, %s, error %d\n", 
                    		__PRETTY_FUNCTION__, (long)this->fDefaultThread, 
                    		fClient->fHost, fUrlM3U8, get_status);
                    	if(fLog != NULL)
                    	{
                    		char str_begin_time[MAX_TIME_LEN] = {0};
                    		char str_end_time[MAX_TIME_LEN] = {0};
							ctime_r(&fM3U8BeginTime.tv_sec, str_begin_time);
							str_begin_time[strlen(str_begin_time)-1] = '\0';
							ctime_r(&fM3U8EndTime.tv_sec, str_end_time);
							str_end_time[strlen(str_end_time)-1] = '\0';
	                   		fprintf(fLog, "%s, source: %s, error: %d, begin: %ld.%06ld [%s], end: %ld.%06ld [%s]\n", 
	                    		fUrlM3U8, fClient->fHost, get_status, 
	                    		fM3U8BeginTime.tv_sec, fM3U8BeginTime.tv_usec,	str_begin_time,
	                    		fM3U8EndTime.tv_sec,   fM3U8EndTime.tv_usec, 	str_end_time);
                    	}
                        theErr = ENOT200; // Exit the state machine
                        break;
                    }
                    else
                    {
                    	fprintf(stdout, "%s[0x%016lX]: %s, %s, ok, len=%d\n", 
                    		__PRETTY_FUNCTION__, (long)this->fDefaultThread, 
                    		fClient->fHost, fUrlM3U8, fClient->GetContentLength());
                    	if(fLog != NULL)
                    	{
                    		char str_begin_time[MAX_TIME_LEN] = {0};
                    		char str_end_time[MAX_TIME_LEN] = {0};
							ctime_r(&fSegmentBeginTime.tv_sec, str_begin_time);
							str_begin_time[strlen(str_begin_time)-1] = '\0';
							ctime_r(&fSegmentEndTime.tv_sec, str_end_time);
							str_end_time[strlen(str_end_time)-1] = '\0';
	                    	fprintf(fLog, "%s, source: %s, len: %8u, begin: %ld.%06ld [%s], end: %ld.%06ld [%s]\n", 
	                    		fUrlM3U8, fClient->fHost, fClient->GetContentLength(), 
	                    		fM3U8BeginTime.tv_sec, fM3U8BeginTime.tv_usec,	str_begin_time,
	                    		fM3U8EndTime.tv_sec,   fM3U8EndTime.tv_usec, 	str_end_time);
                    	}
                    	//Log(fURL.Ptr, fClient->GetContentBody(), fClient->GetContentLength());
                        fM3U8Parser.Parse(fClient->GetContentBody(), fClient->GetContentLength());
                        if(fLog != NULL)
                        {
                        	// before parser.
                        	fprintf(fLog, "%s\n", fM3U8Parser.fData.Ptr);
                        	// after parser. todo
                        }
                        MemoM3U8(&fM3U8Parser, fM3U8BeginTime.tv_sec, fM3U8EndTime.tv_sec);
                        RateSourceM3U8(fM3U8Parser.GetNewestTime(), fClient->GetContentLength(), fM3U8BeginTime, fM3U8EndTime);                        
                        #if 0
                        if(fM3U8Parser.IsOld())
                        {
                        	theErr = ENOTCONN; // Exit the state machine
                        	break;
                        }
                        #endif
                        //RewriteM3U8(&fM3U8Parser);
                        if(!fWithChunkId)
                        {                        	
                        	if(fM3U8Parser.fSegmentsNum <= 0)
			            	{
			            		// source have no segments.			            		
			            		fWithChunkId = false;
			            		fState = kSendingGetM3U8;			            		            		
			            		time_t break_time = CalcBreakTime();
		            			return break_time;
			            	}
			            	else
			            	{
			            		// firstly, we request the url with no chunkid.
			            		// later, we request the url with chunkid. 
			            		fWithChunkId = true;
			            		fState = kSendingGetSegment;
			            	}
                        }
                        else
                        {
	                        if(fM3U8Parser.fSegmentsNum <= 0)
			            	{
			            		// encoder maybe restart.
			            		// so reset fLastChunkId
			            		fLastChunkId = 0;
			            		
			            		fWithChunkId = false;
			            		fState = kSendingGetM3U8;			            			            		
			            		return 10;
			            	}
			            	else if(fM3U8Parser.fSegmentsNum == MAX_SEGMENT_NUM)
			            	{
			            		// i am too late.
			            		fWithChunkId = false;
			            		fState = kSendingGetSegment;
			            	}
			            	else
			            	{
			            		fWithChunkId = true;
			            		fState = kSendingGetSegment;
			            	} 
		            	}
                    }
                }
                
                break;
            }
            case kSendingGetSegment:
            {
            	while(1)
            	{
	            	if(IsDownloaded(&(fM3U8Parser.fSegments[fGetIndex])))
	            	{	
	            		memset(&fGetSegment, 0, sizeof(fGetSegment));
	            		
	            		fGetIndex ++;		            		
	            		if(fGetIndex >= fM3U8Parser.fSegmentsNum)
		            	{
		            		fState = kSendingGetM3U8;	   		            		
		            		time_t break_time = CalcBreakTime();
		            		return break_time;
		            	}		            	
	            	}
	            	else
	            	{	            		
	            		break;
	            	}
            	}            	

				GetSegmentInit();
            	theErr = fClient->SendGetSegment(fM3U8Parser.fSegments[fGetIndex].relative_url, fGetSegment.range_start);
            	fSegmentBeginTime = fClient->fBeginTime;
                fSegmentEndTime = fClient->fEndTime;  
            	if (theErr == OS_NoErr)
                { 
                	UInt32 get_status = fClient->GetStatus();
                	if (get_status != 200 && get_status != 206)
                    {
                    	fprintf(stdout, "%s: %s, %s, error %d\n", 
                    		__PRETTY_FUNCTION__, fClient->fHost, fM3U8Parser.fSegments[fGetIndex].relative_url, get_status);
                    	if(fLog != NULL)
                    	{
                    		char str_begin_time[MAX_TIME_LEN] = {0};
                    		char str_end_time[MAX_TIME_LEN] = {0};
							ctime_r(&fSegmentBeginTime.tv_sec, str_begin_time);
							str_begin_time[strlen(str_begin_time)-1] = '\0';
							ctime_r(&fSegmentEndTime.tv_sec, str_end_time);
							str_end_time[strlen(str_end_time)-1] = '\0';
	                   		fprintf(fLog, "%s, source: %s, error: %d, begin: %ld.%06ld [%s], end: %ld.%06ld [%s]\n", 
	                    		fM3U8Parser.fSegments[fGetIndex].relative_url, fClient->fHost, get_status, 
	                    		fSegmentBeginTime.tv_sec, fSegmentBeginTime.tv_usec,	str_begin_time,
	                    		fSegmentEndTime.tv_sec,   fSegmentEndTime.tv_usec,		str_end_time);
                    	}
                    	
                        fGetIndex ++;
                        // if all the segments downloaded, get m3u8 again
                        if(fGetIndex >= fM3U8Parser.fSegmentsNum)
		            	{
		            		fState = kSendingGetM3U8;
		            	}	                    
	                    						
                        theErr = ENOT200; // Exit the state machine
                        break;                        
                    }
                    else
                    {
                    	fprintf(stdout, "%s: %s, %s, ok, len=%d\n", 
                    		__PRETTY_FUNCTION__, fClient->fHost, fM3U8Parser.fSegments[fGetIndex].relative_url, fClient->GetContentLength());
                    	if(fLog != NULL)
                    	{
                    		char str_begin_time[MAX_TIME_LEN] = {0};
                    		char str_end_time[MAX_TIME_LEN] = {0};
							ctime_r(&fSegmentBeginTime.tv_sec, str_begin_time);
							str_begin_time[strlen(str_begin_time)-1] = '\0';
							ctime_r(&fSegmentEndTime.tv_sec, str_end_time);
							str_end_time[strlen(str_end_time)-1] = '\0';
	                    	fprintf(fLog, "%s, source: %s, len: %8u, begin: %ld.%06ld [%s], end: %ld.%06ld [%s]\n", 
	                    		fM3U8Parser.fSegments[fGetIndex].relative_url, fClient->fHost, fClient->GetContentLength(), 
	                    		fSegmentBeginTime.tv_sec, fSegmentBeginTime.tv_usec, str_begin_time,
	                    		fSegmentEndTime.tv_sec,   fSegmentEndTime.tv_usec, str_end_time);
                    	}  
                    	fLastChunkId = fM3U8Parser.fSegments[fGetIndex].sequence;                    	
                    	//Log(fM3U8Parser.fSegments[fGetIndex].relative_url, fClient->GetContentBody(), fClient->GetContentLength());
                    	MemoSegment(&(fM3U8Parser.fSegments[fGetIndex]), fGetSegment.range_start, fClient->fRangeLength, 
                    		fClient->GetContentBody(), fClient->fContentRecvLen, 
                    		fSegmentBeginTime.tv_sec, fSegmentEndTime.tv_sec);
                    	RateSourceSegment(fClient->GetContentLength(), fSegmentBeginTime, fSegmentEndTime);
                    		                    	
                    	fGetIndex ++;
                        // if all the segments downloaded, get m3u8 again
                        if(fGetIndex >= fM3U8Parser.fSegmentsNum)
		            	{
		            		fState = kSendingGetM3U8;		            		
		            		time_t break_time = CalcBreakTime();
	            			return break_time;
		            	}
                    }
                }
                
                break;
            }
        }
    }

	if ((theErr == EINPROGRESS) || (theErr == EAGAIN))
    {
    	if(ConnectionTimeout())
    	{
    		int result = SwitchSource(theErr);
			if(result < 0) // switch failed, only one source.
			{
				// do nothing.
			}
			else
			{
				fState = kSendingGetM3U8;
			}
			
			time_t break_time = CalcBreakTime();
       		return break_time;
    	}
    	
    	if(DownloadTimeout())
		{		
			if(fClient->fContentRecvLen > 0)
			{
				//save it.
				MemoSegment(&(fM3U8Parser.fSegments[fGetIndex]), fGetSegment.range_start, fClient->fRangeLength, 
						fClient->GetContentBody(), fClient->fContentRecvLen, 
                    	fSegmentBeginTime.tv_sec, fSegmentEndTime.tv_sec);
                fGetSegment.range_start += fClient->fContentRecvLen;                
			}

			fClient->Disconnect();

			fGetSegment.download_count ++;
			if(fGetSegment.download_count <= 3)
			{
				return 10;
			}
			
			MoveSegment();
			
			fGetIndex ++;                        
            // if all the segments downloaded, get m3u8 again
            if(fGetIndex >= fM3U8Parser.fSegmentsNum)
        	{
        		fState = kSendingGetM3U8;
        	}	
			
			int result = SwitchSource(theErr);
			if(result < 0) // switch failed, only one source.
			{
				
			}
			else
			{
				fState = kSendingGetM3U8;
			}
						
       		return 10;
		}
        //
        // Request an async event
        fSocket->GetSocket()->SetTask(this);
        fSocket->GetSocket()->RequestEvent(fSocket->GetEventMask());
    }
    
    else if (theErr != OS_NoErr)
    {
        //
        // We encountered some fatal error with the socket. Record this as a connection failure
        if (fClient->GetStatus() != 200)
            fDeathReason = kRequestFailed;
        else
            fDeathReason = kConnectionFailed;

		if(fLog != NULL)
		{
	   		fprintf(fLog, "we meet error: %d\n", theErr);
		}
		
		int result = SwitchSource(theErr);
		if(result < 0) // switch failed, only one source.
		{
			// do nothing.
		}
		else
		{
			fState = kSendingGetM3U8;
		}
		
		time_t break_time = CalcBreakTime();
       	return break_time;

    }    
		
	return 0;
}




