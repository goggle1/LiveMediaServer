
#include <sys/stat.h>
#include <sys/types.h>
//#include <sys/stat.h>
#include <fcntl.h>

#include "BaseServer/StringParser.h"
#include "BaseServer/StringFormatter.h"

#include "public.h"
#include "HTTPClientSession.h"

// 10 seconds
#define MAX_SEMENT_TIME	5000	

int make_dir(StrPtrLen& dir)
{
	char path[PATH_MAX] = {'\0'};
	sprintf(path, "%s/", ROOT_PATH);
	
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

HTTPClientSession::HTTPClientSession(UInt32 inAddr, UInt16 inPort, const StrPtrLen& inURL, CHANNEL_T* channelp, char* type)
	:fTimeoutTask(this, 60)
{
	fInAddr = inAddr;
	fInPort = inPort;
	fSocket = new TCPClientSocket(Socket::kNonBlockingSocketType);	
	fSocket->Set(fInAddr, fInPort);
	
	fClient = new HTTPClient(fSocket);	

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
	this->Set(inURL);
	this->Signal(Task::kStartEvent);

	fDownloadIndex = 0;
	fGetIndex	= 0;
	memset(fDownloadSegments, 0, sizeof(fDownloadSegments));

	fprintf(stdout, "%s\n", __PRETTY_FUNCTION__);
}

HTTPClientSession::~HTTPClientSession()
{
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

int HTTPClientSession::MemoSegment(char * url, char * datap, UInt32 len)
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
    
	char* relative_url = urlParser.GetCurrentPosition();
	
	SEG_T* segp = &(fMemory->segs[fMemory->seg_index]);	

	strncpy(segp->url, relative_url, MAX_URL_LEN-1);
	segp->url[MAX_URL_LEN-1] = '\0';

	if(segp->data.datap != NULL)
	{
		free(segp->data.datap);
		segp->data.datap = NULL;
	}
	
	segp->data.datap = malloc(len);
	if(segp->data.datap != NULL)
	{
		segp->data.size = len;
		memcpy(segp->data.datap, datap, len);
		segp->data.len = len;
	}

	fMemory->seg_index ++;
	if(fMemory->seg_index >= MAX_SEG_NUM)
	{
		fMemory->seg_index = 0;
	}
	
	fMemory->seg_num ++;
	if(fMemory->seg_num > MAX_SEG_NUM-1)
	{
		fMemory->seg_num = MAX_SEG_NUM-1;
	}
	
    
	return 0;
}

int HTTPClientSession::MemoM3U8(M3U8Parser* parserp)
{
	M3U8_T* m3u8p = &(fMemory->m3u8s[fMemory->m3u8_index]);
	fMemory->m3u8_index ++;
	if(fMemory->m3u8_index >= MAX_M3U8_NUM)
	{
		fMemory->m3u8_index = 0;
	}
	

	if(m3u8p->datap == NULL)
	{	
		m3u8p->datap = malloc(1024);
		m3u8p->size = 1024;		
	}
		
	if(m3u8p->datap != NULL)
	{
		StringFormatter content((char*)m3u8p->datap, m3u8p->size);	
		content.Put("#EXTM3U\n");
		content.PutFmtStr("#EXT-X-TARGETDURATION:%d\n", parserp->fTargetDuration);
		content.PutFmtStr("#EXT-X-MEDIA-SEQUENCE:%d\n", parserp->fMediaSequence);
		int index = 0;
		for(index=0; index<parserp->fSegmentsNum; index++)
		{
			SEGMENT_T* segp = &(parserp->fSegments[index]);
			content.PutFmtStr("#EXTINF:%u,\n", segp->inf);
			content.PutFmtStr("#EXT-X-BYTERANGE:%lu,\n", segp->byte_range);

			StrPtrLen AbsoluteURI(segp->url);
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
		    urlParser.ConsumeUntil(NULL, '/');
		    urlParser.Expect('/');
			content.PutFmtStr("%s\n", urlParser.GetCurrentPosition());
		}
	
		content.PutTerminator();
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
	sprintf(path, "%s/%s_%s.m3u8", ROOT_PATH, fType, fChannel->liveid);
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
	ret = dprintf(fd, "#EXT-X-MEDIA-SEQUENCE:%d\n", parserp->fMediaSequence);
	
	int index = 0;
	for(index=0; index<parserp->fSegmentsNum; index++)
	{
		SEGMENT_T* segp = &(parserp->fSegments[index]);
		ret = dprintf(fd, "#EXTINF:%u,\n", segp->inf);
		ret = dprintf(fd, "#EXT-X-BYTERANGE:%lu,\n", segp->byte_range);

		StrPtrLen AbsoluteURI(segp->url);
		StringParser urlParser(&AbsoluteURI);
	  
	    // we always should have a slash before the URI
	    // If not, that indicates this is a full URI
	    if (AbsoluteURI.Ptr[0] != '/')
	    {
	            //if it is a full URL, store the scheme and host name
	            urlParser.ConsumeLength(NULL, 7); //consume "http://"
	            urlParser.ConsumeUntil(NULL, '/');
	    }    
		ret = dprintf(fd, "%s\n", urlParser.GetCurrentPosition());
	}

	close(fd);
	
	return 0;
}

int HTTPClientSession::Write(StrPtrLen& file_name, char * datap, UInt32 len)
{
	char path[PATH_MAX] = {'\0'};
	sprintf(path, "%s/", ROOT_PATH);
	
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
            	theErr = fClient->SendGetM3U8(fURL.Ptr);
            	if (theErr == OS_NoErr)
                {   
                    if (fClient->GetStatus() != 200)
                    {
                        theErr = ENOTCONN; // Exit the state machine
                        break;
                    }
                    else
                    {
                    	Log(fURL.Ptr, fClient->GetContentBody(), fClient->GetContentLength());
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
            		RewriteM3U8(&fM3U8Parser);
            		MemoM3U8(&fM3U8Parser);
            		return MAX_SEMENT_TIME;
            	}
				
            	while(1)
            	{
	            	if(IsDownloaded(&(fM3U8Parser.fSegments[fGetIndex])))
	            	{
	            		fprintf(stdout, "%s: %s downloaded\n", __PRETTY_FUNCTION__, fM3U8Parser.fSegments[fGetIndex].url);
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
            	
            	theErr = fClient->SendGetSegment(fM3U8Parser.fSegments[fGetIndex].url);
            	if (theErr == OS_NoErr)
                {   
                	if (fClient->GetStatus() != 200)
                    {
                    	if (fClient->GetStatus() == 404)
	                    {
	                        fGetIndex ++;
	                        // if all the segments downloaded, get m3u8 again
	                        if(fGetIndex >= fM3U8Parser.fSegmentsNum)
			            	{
			            		fState = kSendingGetM3U8;
			            		RewriteM3U8(&fM3U8Parser);
			            		MemoM3U8(&fM3U8Parser);
			            		return MAX_SEMENT_TIME;
			            	}
	                    }

                        theErr = ENOTCONN; // Exit the state machine
                        break;
                    }
                    else
                    {
                    	Log(fM3U8Parser.fSegments[fGetIndex].url, fClient->GetContentBody(), fClient->GetContentLength());
                    	MemoSegment(fM3U8Parser.fSegments[fGetIndex].url, fClient->GetContentBody(), fClient->GetContentLength());
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
		            		RewriteM3U8(&fM3U8Parser);
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




