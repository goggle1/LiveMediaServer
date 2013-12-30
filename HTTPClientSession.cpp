
#include <sys/stat.h>
#include <sys/types.h>
//#include <sys/stat.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "BaseServer/StringParser.h"
#include "BaseServer/StringFormatter.h"

#include "public.h"
#include "config.h"
#include "HTTPClientSession.h"

// guoqiang errorcodes
#define ENOT200 	2001
#define ETIMEOUT 	2002

int timeval_cmp(struct timeval* t1, struct timeval* t2)
{
	if(t1->tv_sec < t2->tv_sec)
	{
		return -1;
	}
	else if(t1->tv_sec > t2->tv_sec)
	{
		return 1;
	}
	else // if(t1->tv_sec == t2->tv_sec)
	{
		if(t1->tv_usec < t2->tv_usec)
		{
			return -1;
		}
		else if(t1->tv_usec > t2->tv_usec)
		{
			return 1;
		}
		else // if(t1->tv_usec == t2->tv_usec)
		{
			return 0;
		}
	}
}


time_t timeval_diff(struct timeval* t2, struct timeval* t1)
{
	time_t ret1 = 0;
	time_t ret2 = 0;
	ret1 = t2->tv_sec - t1->tv_sec;
	ret2 = ret1 * 1000 + (t2->tv_usec - t1->tv_usec)/1000;
	return ret2;
}


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
	SetSources(fChannel->source_list);

	strncpy(fLiveType, live_type, MAX_LIVE_TYPE-1);
	fLiveType[MAX_LIVE_TYPE-1] = '\0';
	strcpy(fM3U8Path, "livestream");
	//snprintf(fUrl, MAX_URL_LEN, "/%s/%s.m3u8?codec=%s", fM3U8Path, channelp->liveid, live_type);
	snprintf(fUrl, MAX_URL_LEN, "/%s/%s.m3u8?codec=%s&len=%d", fM3U8Path, channelp->liveid, live_type, MAX_SEGMENT_NUM);
	fUrl[MAX_URL_LEN-1] = '\0';	
	StrPtrLen path(fM3U8Path);
	fM3U8Parser.SetPath(&path);

	fState = kSendingGetM3U8;	
	fGetIndex	= 0;
	fWillStop = false;
	fWillUpdateSources = false;

	gettimeofday(&fBeginTime, NULL);
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
	
	//this->Signal(Task::kStartEvent);

	
}

HTTPClientSession::~HTTPClientSession()
{	
	gettimeofday(&fEndTime, NULL);	
	if(fLog != NULL)
	{
		fprintf(fLog, "%s, end_time: %ld.%06ld\n", __PRETTY_FUNCTION__, fEndTime.tv_sec, fEndTime.tv_usec);
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
	time_t day1 = fLogTime.tv_sec / (3600*24);
	time_t day2 = until.tv_sec / (3600*24);
	if(day1 == day2)
	{
		return -1;
	}

	fLogTime = until;
	if(fLog != NULL)
	{
		fclose(fLog);
		fLog = NULL;
	}

	fLogTime = until;
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
	
	fState = kSendingGetM3U8;	
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
	 
	ret = fClient->SetSource(sourcep->ip, sourcep->port);

	return ret;
}

Bool16 HTTPClientSession::DownloadTimeout()
{
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
	if(diff_time > MAX_TIMEOUT_TIME)
	{
		fprintf(stdout, "%s: LiveType=%s, LiveId=%s, EndTime=%ld, BeginTime=%ld\n", 
			__PRETTY_FUNCTION__, fLiveType, fChannel->liveid, fClient->fEndTime.tv_sec, fClient->fBeginTime.tv_sec);	
		return true;
	}
	
	return false;
}

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

int HTTPClientSession::MemoSourceM3U8(time_t newest_time, u_int64_t download_bytes, struct timeval download_begin_time, struct timeval download_end_time)
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

int HTTPClientSession::MemoSourceSegment(u_int64_t download_bytes, struct timeval download_begin_time, struct timeval download_end_time)
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


int HTTPClientSession::MemoSegment(SEGMENT_T* onep, char * datap, UInt32 len, time_t begin_time, time_t end_time)
{	
	CLIP_T* clipp = &(fMemory->clips[fMemory->clip_index]);	

	clipp->inf = onep->inf;
	clipp->byte_range = onep->byte_range;
	clipp->sequence = onep->sequence;	
	
	strncpy(clipp->relative_url, onep->relative_url, MAX_URL_LEN-1);
	clipp->relative_url[MAX_URL_LEN-1] = '\0';	

	strncpy(clipp->m3u8_relative_url, onep->m3u8_relative_url, MAX_URL_LEN-1);
	clipp->m3u8_relative_url[MAX_URL_LEN-1] = '\0';

	strncpy(clipp->file_name, onep->file_name, MAX_URL_LEN-1);
	clipp->file_name[MAX_URL_LEN-1] = '\0';

	if(clipp->data.datap != NULL)
	{
		free(clipp->data.datap);
		clipp->data.datap = NULL;
	}
	
	clipp->data.datap = malloc(len);
	if(clipp->data.datap != NULL)
	{
		clipp->data.size = len;
		memcpy(clipp->data.datap, datap, len);
		clipp->data.len = len;
	}

	clipp->begin_time = begin_time;
	clipp->end_time = end_time;

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
	else
	{
		break_time = g_config.download_interval - diff_time;
	}
	fprintf(stdout, "%s: diff_time=%ld, break_time=%ld\n", 
		__PRETTY_FUNCTION__, diff_time, break_time);
	return break_time;
}

SInt64 HTTPClientSession::Run()
{	
	Task::EventFlags theEvents = this->GetEvents(); 
	
	if (theEvents & Task::kStartEvent)
    {
		SwitchLog(fBeginTime);
    }

	#if 0
	if (theEvents & Task::kTimeoutEvent)
	{
		if(fState == kDone)
			return 0;
			
	    fDeathReason = kSessionTimedout;
	    fState = kDone;
	    return 0;
	}
	#endif

    // We have been told to delete ourselves. Do so... NOW!!!!!!!!!!!!!!!
    if (theEvents & Task::kKillEvent)
    {
        return -1;
    }

    // Refresh the timeout. There is some legit activity going on...
    #if 0
    fTimeoutTask.RefreshTimeout();
    #endif
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
            	fGetTryCount = 0;
            	//fprintf(stdout, "%s[0x%016lX][0x%016lX][%ld]: get %s\n", __PRETTY_FUNCTION__, this->fDefaultThread, this->fUseThisThread, pthread_self(), fURL.Ptr);            	
            	theErr = fClient->SendGetM3U8(fUrl); 
            	fM3U8BeginTime = fClient->fBeginTime;
               	fM3U8EndTime = fClient->fEndTime;               	
            	if (theErr == OS_NoErr)
                {                   	
                	UInt32 get_status = fClient->GetStatus();
                    if (get_status != 200)
                    {
                    	fprintf(stdout, "%s[0x%016lX][0x%016lX][%ld]: get %s return error: %d\n", 
                    		__PRETTY_FUNCTION__, (long)this->fDefaultThread, (long)this->fUseThisThread, pthread_self(), 
                    		fUrl, get_status);
                        theErr = ENOT200; // Exit the state machine
                        break;
                    }
                    else
                    {
                    	fprintf(stdout, "%s[0x%016lX][0x%016lX][%ld]: get %s done, len=%d\n", 
                    		__PRETTY_FUNCTION__, (long)this->fDefaultThread, (long)this->fUseThisThread, pthread_self(), 
                    		fUrl, fClient->GetContentLength());
                    	//Log(fURL.Ptr, fClient->GetContentBody(), fClient->GetContentLength());
                        fM3U8Parser.Parse(fClient->GetContentBody(), fClient->GetContentLength());
                        MemoSourceM3U8(fM3U8Parser.GetNewestTime(), fClient->GetContentLength(), fM3U8BeginTime, fM3U8EndTime);
                        #if 0
                        if(fM3U8Parser.IsOld())
                        {
                        	theErr = ENOTCONN; // Exit the state machine
                        	break;
                        }
                        #endif
                        //RewriteM3U8(&fM3U8Parser);
                        fState = kSendingGetSegment;
                    }
                }
                
                break;
            }
            case kSendingGetSegment:
            {
            	if(fM3U8Parser.fSegmentsNum <= 0)
            	{
            		fState = kSendingGetM3U8;
            		//RewriteM3U8(&fM3U8Parser);
					MemoM3U8(&fM3U8Parser, fM3U8BeginTime.tv_sec, fM3U8EndTime.tv_sec);
            		time_t break_time = CalcBreakTime();
            		return break_time;
            	}
				
            	while(1)
            	{
	            	if(IsDownloaded(&(fM3U8Parser.fSegments[fGetIndex])))
	            	{
	            		//fprintf(stdout, "%s: %s downloaded\n", __PRETTY_FUNCTION__, fM3U8Parser.fSegments[fGetIndex].relative_url);
	            		fGetIndex ++;
	            		fGetTryCount = 0;
	            		if(fGetIndex >= fM3U8Parser.fSegmentsNum)
		            	{
		            		fState = kSendingGetM3U8;	   
		            		MemoM3U8(&fM3U8Parser, fM3U8BeginTime.tv_sec, fM3U8EndTime.tv_sec);
		            		time_t break_time = CalcBreakTime();
		            		return break_time;
		            	}
	            	}
	            	else
	            	{	            		
	            		break;
	            	}
            	}            	

            	//fprintf(stdout, "%s: get %s\n", __PRETTY_FUNCTION__, fM3U8Parser.fSegments[fGetIndex].relative_url);
            	theErr = fClient->SendGetSegment(fM3U8Parser.fSegments[fGetIndex].relative_url);
            	fSegmentBeginTime = fClient->fBeginTime;
                fSegmentEndTime = fClient->fEndTime;  
            	if (theErr == OS_NoErr)
                {  
                	SwitchLog(fSegmentBeginTime);
                	UInt32 get_status = fClient->GetStatus();
                	if (get_status != 200)
                    {
                    	fprintf(stdout, "%s: get %s return error: %d\n", 
                    		__PRETTY_FUNCTION__, fM3U8Parser.fSegments[fGetIndex].relative_url, get_status);
                    	if(fLog != NULL)
                    	{
                    		char str_begin_time[MAX_TIME_LEN] = {0};
                    		char str_end_time[MAX_TIME_LEN] = {0};
							ctime_r(&fSegmentBeginTime.tv_sec, str_begin_time);
							str_begin_time[strlen(str_begin_time)-1] = '\0';
							ctime_r(&fSegmentEndTime.tv_sec, str_end_time);
							str_end_time[strlen(str_end_time)-1] = '\0';
	                   		fprintf(fLog, "%s, error: %d, begin: %ld.%06ld [%s], end: %ld.%06ld [%s]\n", 
	                    		fM3U8Parser.fSegments[fGetIndex].relative_url, get_status, 
	                    		fSegmentBeginTime.tv_sec, fSegmentBeginTime.tv_usec, str_begin_time,
	                    		fSegmentEndTime.tv_sec,   fSegmentEndTime.tv_usec, str_end_time);
                    	}
                    	
                        fGetIndex ++;
                        fGetTryCount = 0;
                        // if all the segments downloaded, get m3u8 again
                        if(fGetIndex >= fM3U8Parser.fSegmentsNum)
		            	{
		            		fState = kSendingGetM3U8;
		            		//RewriteM3U8(&fM3U8Parser);
							MemoM3U8(&fM3U8Parser, fM3U8BeginTime.tv_sec, fM3U8EndTime.tv_sec);
		            	}	                    
	                    						
                        theErr = ENOT200; // Exit the state machine
                        break;                        
                    }
                    else
                    {
                    	fprintf(stdout, "%s: get %s done, len=%d\n", 
                    		__PRETTY_FUNCTION__, fM3U8Parser.fSegments[fGetIndex].relative_url, fClient->GetContentLength());
                    	if(fLog != NULL)
                    	{
                    		char str_begin_time[MAX_TIME_LEN] = {0};
                    		char str_end_time[MAX_TIME_LEN] = {0};
							ctime_r(&fSegmentBeginTime.tv_sec, str_begin_time);
							str_begin_time[strlen(str_begin_time)-1] = '\0';
							ctime_r(&fSegmentEndTime.tv_sec, str_end_time);
							str_end_time[strlen(str_end_time)-1] = '\0';
	                    	fprintf(fLog, "%s, len: %u, begin: %ld.%06ld [%s], end_time: %ld.%06ld [%s]\n", 
	                    		fM3U8Parser.fSegments[fGetIndex].relative_url, fClient->GetContentLength(), 
	                    		fSegmentBeginTime.tv_sec, fSegmentBeginTime.tv_usec, str_begin_time,
	                    		fSegmentEndTime.tv_sec,   fSegmentEndTime.tv_usec, str_end_time);
                    	}
                    	MemoSourceSegment(fClient->GetContentLength(), fSegmentBeginTime, fSegmentEndTime);
                    	if(fClient->GetContentLength() > 0)
                    	{
	                    	//Log(fM3U8Parser.fSegments[fGetIndex].relative_url, fClient->GetContentBody(), fClient->GetContentLength());
	                    	MemoSegment(&(fM3U8Parser.fSegments[fGetIndex]), fClient->GetContentBody(), fClient->GetContentLength(), 
	                    		fSegmentBeginTime.tv_sec, fSegmentEndTime.tv_sec);
	                    		                    	
	                    	fGetIndex ++;
	                    	fGetTryCount = 0;
	                        // if all the segments downloaded, get m3u8 again
	                        if(fGetIndex >= fM3U8Parser.fSegmentsNum)
			            	{
			            		fState = kSendingGetM3U8;
			            		//RewriteM3U8(&fM3U8Parser);
			            		MemoM3U8(&fM3U8Parser, fM3U8BeginTime.tv_sec, fM3U8EndTime.tv_sec);			            		
			            		time_t break_time = CalcBreakTime();
		            			return break_time;
			            	}
		            	}		            
		            	else // get_status==200 but content-length == 0,
		            	{
		            		//theErr = ENOTCONN; // Exit the state machine
                        	//break;
		            		fGetTryCount ++;
		            		if(fGetTryCount >= MAX_TRY_COUNT)
		            		{
		            			MemoSegment(&(fM3U8Parser.fSegments[fGetIndex]), fClient->GetContentBody(), fClient->GetContentLength(), 
	                    			fSegmentBeginTime.tv_sec, fSegmentEndTime.tv_sec);
		                    	
		                    	fGetIndex ++;
		                    	fGetTryCount = 0;
		                        // if all the segments downloaded, get m3u8 again
		                        if(fGetIndex >= fM3U8Parser.fSegmentsNum)
				            	{
				            		fState = kSendingGetM3U8;
				            		//RewriteM3U8(&fM3U8Parser);
				            		MemoM3U8(&fM3U8Parser, fM3U8BeginTime.tv_sec, fM3U8EndTime.tv_sec);
				            		time_t break_time = CalcBreakTime();
			            			return break_time;
				            	}
		            		}
		            		return g_config.download_interval/2;	            		
		            	}
                    }
                }
                
                break;
            }
        }
    }

	if ((theErr == EINPROGRESS) || (theErr == EAGAIN))
    {
    	if(DownloadTimeout())
		{
			theErr = ETIMEOUT; // Exit the state machine
			SwitchSource(theErr);
			time_t break_time = CalcBreakTime();
       		return break_time;
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
		
		//fState = kDone;
		SwitchSource(theErr);		
		
		time_t break_time = CalcBreakTime();
       	return break_time;

    }    
	
	return 0;
}




