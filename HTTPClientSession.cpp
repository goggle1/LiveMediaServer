
#include <sys/stat.h>
#include <sys/types.h>
//#include <sys/stat.h>
#include <fcntl.h>

#include "BaseServer/StringParser.h"
#include "BaseServer/StringFormatter.h"

#include "public.h"
#include "config.h"
#include "HTTPClientSession.h"

// 10 seconds
#define MAX_SEMENT_TIME	5000	

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
	//fSocket->Set(fInAddr, fInPort);
	
	fClient = new HTTPClient(fSocket, channelp);	

	fChannel= channelp;
	fType	= strdup(type);
	fMemory = (MEMORY_T*)malloc(sizeof(MEMORY_T));	
	memset(fMemory, 0, sizeof(MEMORY_T));
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
	this->Set(inURL);
	this->Signal(Task::kStartEvent);

	fDownloadIndex = 0;
	fGetIndex	= 0;
	memset(fDownloadSegments, 0, sizeof(fDownloadSegments));

	fprintf(stdout, "%s\n", __PRETTY_FUNCTION__);
}

HTTPClientSession::~HTTPClientSession()
{	
	delete [] fM3U8Path.Ptr;
	delete [] fURL.Ptr;

	if(fMemory != NULL)
	{
		int index = 0;
		for(index=0; index<MAX_M3U8_NUM; index++)
		{
			if(fMemory->m3u8s[index].datap != NULL)
			{
				fMemory->m3u8s[index].len = 0;
				fMemory->m3u8s[index].size = 0;
				free(fMemory->m3u8s[index].datap);				
			}
		}
		for(index=0; index<MAX_CLIP_NUM; index++)
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

void HTTPClientSession::Set(const StrPtrLen& inURL)
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


Bool16 HTTPClientSession::IsDownloaded(SEGMENT_T * segp)
{
	int index = 0;
	for(index=0; index<MAX_SEGMENT_NUM; index++)
	{
		if(strcmp(fDownloadSegments[index].url, segp->url) == 0)
		{
			return true;
		}
	}

	return false;
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

int HTTPClientSession::MemoSegment(SEGMENT_T* onep, char * datap, UInt32 len)
{	
	CLIP_T* clipp = &(fMemory->clips[fMemory->clip_index]);	

	clipp->inf = onep->inf;
	clipp->byte_range = onep->byte_range;
	clipp->sequence = onep->sequence;	
	
	strncpy(clipp->relative_url, onep->relative_url, MAX_URL_LEN-1);
	clipp->relative_url[MAX_URL_LEN-1] = '\0';	

	strncpy(clipp->m3u8_relative_url, onep->m3u8_relative_url, MAX_URL_LEN-1);
	clipp->m3u8_relative_url[MAX_URL_LEN-1] = '\0';

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

	fMemory->clip_index ++;
	if(fMemory->clip_index >= MAX_CLIP_NUM)
	{
		fMemory->clip_index = 0;
	}
	
	fMemory->clip_num ++;
	if(fMemory->clip_num > MAX_CLIP_NUM-1)
	{
		fMemory->clip_num = MAX_CLIP_NUM-1;
	}	
    
	return 0;
}

int HTTPClientSession::MemoM3U8(M3U8Parser* parserp)
{
	fMemory->target_duration = parserp->fTargetDuration;
	
	M3U8_T* m3u8p = &(fMemory->m3u8s[fMemory->m3u8_index]);
	fMemory->m3u8_index ++;
	if(fMemory->m3u8_index >= MAX_M3U8_NUM)
	{
		fMemory->m3u8_index = 0;
	}
	

	if(m3u8p->datap == NULL)
	{	
		m3u8p->datap = malloc(MAX_M3U8_CONTENT_LEN);
		m3u8p->size = MAX_M3U8_CONTENT_LEN;		
	}
		
	if(m3u8p->datap != NULL)
	{
		StringFormatter content((char*)m3u8p->datap, m3u8p->size);	
		content.Put("#EXTM3U\n");
		content.PutFmtStr("#EXT-X-TARGETDURATION:%d\n", parserp->fTargetDuration);
		content.PutFmtStr("#EXT-X-MEDIA-SEQUENCE:%lu\n", parserp->fMediaSequence);
		int index = 0;
		for(index=0; index<parserp->fSegmentsNum; index++)
		{
			SEGMENT_T* segp = &(parserp->fSegments[index]);
			content.PutFmtStr("#EXTINF:%u,\n", segp->inf);
			content.PutFmtStr("#EXT-X-BYTERANGE:%lu,\n", segp->byte_range);
			content.PutFmtStr("%s\n", segp->m3u8_relative_url);
		}
	
		//content.PutTerminator();
		m3u8p->len = content.GetBytesWritten();
	}
	
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

SInt64 HTTPClientSession::Run()
{	
	Task::EventFlags theEvents = this->GetEvents(); 
	
	if (theEvents & Task::kStartEvent)
    {
    	// todo:
    }

	/*
    if (theEvents & Task::kTimeoutEvent)
    {
		if(fState == kDone)
			return 0;
			
        fDeathReason = kSessionTimedout;
        fState = kDone;
        return 0;
    }
    */

    // We have been told to delete ourselves. Do so... NOW!!!!!!!!!!!!!!!
    if (theEvents & Task::kKillEvent)
    {
        return -1;
    }

    // Refresh the timeout. There is some legit activity going on...
    fTimeoutTask.RefreshTimeout();

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
            	//fprintf(stdout, "%s[0x%016lX][0x%016lX][%ld]: get %s\n", __PRETTY_FUNCTION__, this->fDefaultThread, this->fUseThisThread, pthread_self(), fURL.Ptr);
            	theErr = fClient->SendGetM3U8(fURL.Ptr);            	
            	if (theErr == OS_NoErr)
                {   
                	UInt32 get_status = fClient->GetStatus();
                    if (get_status != 200)
                    {
                    	fprintf(stdout, "%s[0x%016lX][0x%016lX][%ld]: get %s return error: %d\n", 
                    		__PRETTY_FUNCTION__, (long)this->fDefaultThread, (long)this->fUseThisThread, pthread_self(), 
                    		fURL.Ptr, get_status);
                        theErr = ENOTCONN; // Exit the state machine
                        break;
                    }
                    else
                    {
                    	fprintf(stdout, "%s[0x%016lX][0x%016lX][%ld]: get %s done\n", 
                    		__PRETTY_FUNCTION__, (long)this->fDefaultThread, (long)this->fUseThisThread, pthread_self(), 
                    		fURL.Ptr);
                    	//Log(fURL.Ptr, fClient->GetContentBody(), fClient->GetContentLength());
                        fM3U8Parser.Parse(fClient->GetContentBody(), fClient->GetContentLength());
                        //RewriteM3U8(&fM3U8Parser);
                        fGetIndex = 0;
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
            		MemoM3U8(&fM3U8Parser);
            		return MAX_SEMENT_TIME;
            	}
				
            	while(1)
            	{
	            	if(IsDownloaded(&(fM3U8Parser.fSegments[fGetIndex])))
	            	{
	            		fprintf(stdout, "%s: %s downloaded\n", __PRETTY_FUNCTION__, fM3U8Parser.fSegments[fGetIndex].relative_url);
	            		fGetIndex ++;
	            		if(fGetIndex >= fM3U8Parser.fSegmentsNum)
		            	{
		            		fState = kSendingGetM3U8;		            		
		            		return MAX_SEMENT_TIME;
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
                	UInt32 get_status = fClient->GetStatus();
                	if (get_status != 200)
                    {
                    	fprintf(stdout, "%s: get %s return error: %d\n", __PRETTY_FUNCTION__, fM3U8Parser.fSegments[fGetIndex].relative_url, get_status);
                    	if (get_status == 404)
	                    {
	                        fGetIndex ++;
	                        // if all the segments downloaded, get m3u8 again
	                        if(fGetIndex >= fM3U8Parser.fSegmentsNum)
			            	{
			            		fState = kSendingGetM3U8;
			            		//RewriteM3U8(&fM3U8Parser);
			            		MemoM3U8(&fM3U8Parser);
			            		return MAX_SEMENT_TIME;
			            	}
	                    }

                        theErr = ENOTCONN; // Exit the state machine
                        break;
                    }
                    else
                    {
                    	fprintf(stdout, "%s: get %s done\n", __PRETTY_FUNCTION__, fM3U8Parser.fSegments[fGetIndex].relative_url);
                    	//Log(fM3U8Parser.fSegments[fGetIndex].relative_url, fClient->GetContentBody(), fClient->GetContentLength());
                    	MemoSegment(&(fM3U8Parser.fSegments[fGetIndex]), fClient->GetContentBody(), fClient->GetContentLength());
                    	memcpy(&(fDownloadSegments[fDownloadIndex]), &(fM3U8Parser.fSegments[fGetIndex]), sizeof(SEGMENT_T));
                    	fDownloadIndex ++;
                    	if(fDownloadIndex >= MAX_SEGMENT_NUM)
                    	{
                    		fDownloadIndex = 0;
                    	}
                    	
                    	fGetIndex ++;
                        // if all the segments downloaded, get m3u8 again
                        if(fGetIndex >= fM3U8Parser.fSegmentsNum)
		            	{
		            		fState = kSendingGetM3U8;
		            		//RewriteM3U8(&fM3U8Parser);
		            		MemoM3U8(&fM3U8Parser);
		            		return MAX_SEMENT_TIME;
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

		//this->Reconnect();
		
        //fState = kDone;
        return MAX_SEMENT_TIME;
    }    
	
	return 0;
}




