
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
	sprintf(path, "%s/", g_config.work_path);
	
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

HTTPClientSession::HTTPClientSession(const StrPtrLen& inURL, CHANNEL_T* channelp, char* type)
	:fTimeoutTask(this, 60)
{		
	fSocket = new TCPClientSocket(Socket::kNonBlockingSocketType);	
	//fSocket->SetUrl(fInAddr, fInPort);
	
	fClient = new HTTPClient(fSocket/*, channelp*/);	

	fChannel= channelp;
	SetSources(fChannel->source_list);
	
	fType	= strdup(type);
	fMemory = (MEMORY_T*)malloc(sizeof(MEMORY_T));	
	memset(fMemory, 0, sizeof(MEMORY_T));
	fMemory->clips = (CLIP_T*)malloc(g_config.max_clip_num * sizeof(CLIP_T));
	memset(fMemory->clips, 0, g_config.max_clip_num * sizeof(CLIP_T));
	if(strcasecmp(fType, "ts") == 0)
	{
		channelp->memoryp_ts = fMemory;
	}
	else if(strcasecmp(fType, "flv") == 0)
	{
		channelp->memoryp_flv = fMemory;
	}
	else if(strcasecmp(fType, "mp4") == 0)
	{
		channelp->memoryp_mp4 = fMemory;
	}

	fState = kSendingGetM3U8;
	this->SetUrl(inURL);	
	
	fGetIndex	= 0;

	snprintf(fLogFile, PATH_MAX, "%s/%s_%s.log", g_config.work_path, fType, fChannel->liveid);
	fLogFile[PATH_MAX-1] = '\0';
	fLogFilep = fopen(fLogFile, "a");
	//this->Signal(Task::kStartEvent);

	fprintf(stdout, "%s\n", __PRETTY_FUNCTION__);
}

HTTPClientSession::~HTTPClientSession()
{	
	if(fLogFilep != NULL)
	{
		fclose(fLogFilep);
		fLogFilep = NULL;
	}
	
	delete [] fM3U8Path.Ptr;
	delete [] fURL.Ptr;

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
	
	if(fType != NULL)
	{
		free(fType);
		fType = NULL;
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

void HTTPClientSession::SetUrl(const StrPtrLen& inURL)
{
    delete [] fURL.Ptr;
    fURL.Ptr = new char[inURL.Len + 2];
    fURL.Len = inURL.Len;
    char* destPtr = fURL.Ptr;
    
    // add a leading '/' to the url if it isn't a full URL and doesn't have a leading '/'
    if ( !inURL.NumEqualIgnoreCase("http://", strlen("http://")) && inURL.Ptr[0] != '/')
    {
        *destPtr = '/';
        destPtr++;
        fURL.Len++;
    }
    ::memcpy(destPtr, inURL.Ptr, inURL.Len);
    fURL.Ptr[fURL.Len] = '\0';

    // get m3u8 path    
    delete [] fM3U8Path.Ptr;
    int path_len = 0;
    int index = fURL.Len;
    for(index=fURL.Len; index>=1; index--)
    {
    	if(fURL.Ptr[index] == '/')
    	{
    		path_len = index - 1;
    	}
    }
    fM3U8Path.Ptr = new char[path_len+1];
    fM3U8Path.Len = path_len;
    ::memcpy(fM3U8Path.Ptr, fURL.Ptr+1, path_len);
    fM3U8Path.Ptr[path_len] = '\0';   

    fM3U8Parser.SetPath(&fM3U8Path);
    
}

int HTTPClientSession::SetSources(DEQUE_NODE* source_list)
{
	int ret = 0;

	fprintf(stdout, "%s\n", __PRETTY_FUNCTION__);
	
	fState = kSendingGetM3U8;	
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

int HTTPClientSession::SwitchSource(OS_Error theErr)
{
	int ret = 0;
	
	//ENOT200
	//ETIMEOUT
	// else
	// network errno
	
	fprintf(stdout, "%s\n", __PRETTY_FUNCTION__);	
	if(fSourceNum <= 1)
	{
		// network errno
		if(theErr != ENOT200 && theErr != ETIMEOUT)
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
	// todo
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
			content.PutFmtStr("#EXT-X-BYTERANGE:%lu\n", segp->byte_range);
			#if 0
			content.PutFmtStr("%s\n", segp->m3u8_relative_url);
			#else
			content.PutFmtStr("http://%s:%u/%s/%s\n", g_config.service_ip, g_config.port, fM3U8Path.Ptr, segp->m3u8_relative_url);
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
	sprintf(path, "%s/%s_%s.m3u8", g_config.work_path, fType, fChannel->liveid);
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
		ret = dprintf(fd, "#EXT-X-BYTERANGE:%lu,\n", segp->byte_range);
		ret = dprintf(fd, "%s\n", segp->relative_url);
	}

	close(fd);
	
	return 0;
}

int HTTPClientSession::Write(StrPtrLen& file_name, char * datap, UInt32 len)
{
	char path[PATH_MAX] = {'\0'};
	sprintf(path, "%s/", g_config.work_path);
	
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
	time_t break_time = MAX_SEMENT_TIME;			            		
	if(diff_time>=MAX_SEMENT_TIME)
	{
		break_time = 1;
	}
	else
	{
		break_time = MAX_SEMENT_TIME - diff_time;
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
    	// do nothing.
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

    OS_Error theErr = OS_NoErr;    
    while ((theErr == OS_NoErr) && (fState != kDone))
    {
        //
        // Do the appropriate thing depending on our current state
        switch (fState)
        {
            case kSendingGetM3U8:
            {
            	fGetIndex = 0;
            	fGetTryCount = 0;
            	//fprintf(stdout, "%s[0x%016lX][0x%016lX][%ld]: get %s\n", __PRETTY_FUNCTION__, this->fDefaultThread, this->fUseThisThread, pthread_self(), fURL.Ptr);            	
            	theErr = fClient->SendGetM3U8(fURL.Ptr);            	
            	if (theErr == OS_NoErr)
                {   
                	fM3U8BeginTime = fClient->fBeginTime;
                	fM3U8EndTime = fClient->fEndTime;
                	UInt32 get_status = fClient->GetStatus();
                    if (get_status != 200)
                    {
                    	fprintf(stdout, "%s[0x%016lX][0x%016lX][%ld]: get %s return error: %d\n", 
                    		__PRETTY_FUNCTION__, (long)this->fDefaultThread, (long)this->fUseThisThread, pthread_self(), 
                    		fURL.Ptr, get_status);
                        theErr = ENOT200; // Exit the state machine
                        break;
                    }
                    else
                    {
                    	fprintf(stdout, "%s[0x%016lX][0x%016lX][%ld]: get %s done, len=%d\n", 
                    		__PRETTY_FUNCTION__, (long)this->fDefaultThread, (long)this->fUseThisThread, pthread_self(), 
                    		fURL.Ptr, fClient->GetContentLength());
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
	            		fprintf(stdout, "%s: %s downloaded\n", __PRETTY_FUNCTION__, fM3U8Parser.fSegments[fGetIndex].relative_url);
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
            	if (theErr == OS_NoErr)
                {   
                	fSegmentBeginTime = fClient->fBeginTime;
                	fSegmentEndTime = fClient->fEndTime;                	
                	UInt32 get_status = fClient->GetStatus();
                	if (get_status != 200)
                    {
                    	fprintf(stdout, "%s: get %s return error: %d\n", 
                    		__PRETTY_FUNCTION__, fM3U8Parser.fSegments[fGetIndex].relative_url, get_status);
                   		fprintf(fLogFilep, "%s, error: %d, begin_time: %ld.%ld, end_time: %ld.%ld\n", 
                    		fM3U8Parser.fSegments[fGetIndex].relative_url, get_status, 
                    		fSegmentBeginTime.tv_sec, fSegmentBeginTime.tv_usec,
                    		fSegmentEndTime.tv_sec,   fSegmentEndTime.tv_usec);
                    	
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
                    	fprintf(fLogFilep, "%s, len: %u, begin_time: %ld.%ld, end_time: %ld.%ld\n", 
                    		fM3U8Parser.fSegments[fGetIndex].relative_url, fClient->GetContentLength(), 
                    		fSegmentBeginTime.tv_sec, fSegmentBeginTime.tv_usec,
                    		fSegmentEndTime.tv_sec,   fSegmentEndTime.tv_usec);
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
			            		if(DownloadTimeout())
			            		{
			            			theErr = ETIMEOUT; // Exit the state machine
                        			break; 
			            		}
			            		time_t break_time = CalcBreakTime();
		            			return break_time;
			            	}
			            	if(DownloadTimeout())
		            		{
		            			theErr = ETIMEOUT; // Exit the state machine
                    			break; 
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
		            		return MAX_SEMENT_TIME/2;	            		
		            	}
                    }
                }
                
                break;
            }
        }
    }

	if ((theErr == EINPROGRESS) || (theErr == EAGAIN))
    {
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

		time_t break_time = 1;
		int switch_ret = SwitchSource(theErr);
        if(switch_ret == 0)
        {    
        	if(theErr == ENOT200)
        	{
				break_time = CalcBreakTime();	
			}
			else if(theErr == ETIMEOUT)
			{
				break_time = CalcBreakTime();	
			}
			else
			{
				break_time = 1;
			}
        }
        else 
        {
        	break_time = CalcBreakTime();			
        }

       	return break_time;

    }    
	
	return 0;
}




