#include <stdio.h>
#include <stdlib.h>

#include "BaseServer/StringParser.h"

#include "M3U8Parser.h"

M3U8Parser::M3U8Parser()
{
}


M3U8Parser::~M3U8Parser()
{
	if(fData.Ptr != NULL)
	{
		delete []fData.Ptr;
		fData.Ptr = NULL;
		fData.Len = 0;
	}
}

int M3U8Parser::SetPath(StrPtrLen* pathp)
{
	if(fM3U8Path.Ptr != NULL)
	{
		delete []fM3U8Path.Ptr;
		fM3U8Path.Ptr = NULL;
		fM3U8Path.Len = 0;
	}

	char *dataCopy = new char[pathp->Len+1];  
    memcpy(dataCopy, pathp->Ptr, pathp->Len);
    dataCopy[pathp->Len] = '\0';

    fM3U8Path.Set(dataCopy, pathp->Len);
	
	return 0;
}

int M3U8Parser::Parse(char * datap, UInt32 len)
{
	if(fData.Ptr != NULL)
	{
		delete []fData.Ptr;
		fData.Ptr = NULL;
		fData.Len = 0;
	}
	
	char *dataCopy = new char[len+1];  
    memcpy(dataCopy, datap, len);
    dataCopy[len] = '\0';
    fData.Set(dataCopy, len);

	/*
    	#EXTM3U
	#EXT-X-TARGETDURATION:10
	#EXT-X-MEDIA-SEQUENCE:565631
	#EXTINF:10,
	#EXT-X-BYTERANGE:1095852
	http://lm.funshion.com/livestream/3702892333/fd5f6b86b836e38c8eed27c9e66e3e6dcf0a69b2/ts/2013/10/25/20131017T174027_03_20131024_155801_565631.ts
	#EXTINF:10,
	#EXT-X-BYTERANGE:1113712
	http://lm.funshion.com/livestream/3702892333/fd5f6b86b836e38c8eed27c9e66e3e6dcf0a69b2/ts/2013/10/25/20131017T174027_03_20131024_155811_565632.ts
	#EXTINF:10,
	#EXT-X-BYTERANGE:1101492
	http://lm.funshion.com/livestream/3702892333/fd5f6b86b836e38c8eed27c9e66e3e6dcf0a69b2/ts/2013/10/25/20131017T174027_03_20131024_155821_565633.ts
	*/
	
	fSegmentsNum = 0;
	memset(fSegments, 0, sizeof(fSegments));
	
	StrPtrLen oneLine;
	StringParser dataParser(&fData);		
	while (dataParser.GetDataRemaining() > 0)
	{
		dataParser.GetThruEOL(&oneLine);
		if (oneLine.Len == 0)
			continue; //skip over any blank lines
		if (oneLine.Ptr[0] == '\0')
			continue; // skip '\0'
	
		if(strncasecmp(oneLine.Ptr, "#EXTM3U", strlen("#EXTM3U")) == 0)
		{
			// do nothing.
		}
		else if(strncasecmp(oneLine.Ptr, "#EXT-X-TARGETDURATION:", strlen("#EXT-X-TARGETDURATION:")) == 0)
		{
			fTargetDuration = atoi(oneLine.Ptr+strlen("#EXT-X-TARGETDURATION:"));
		}
		else if(strncasecmp(oneLine.Ptr, "#EXT-X-MEDIA-SEQUENCE:", strlen("#EXT-X-MEDIA-SEQUENCE:")) == 0)
		{
			fMediaSequence = atol(oneLine.Ptr+strlen("#EXT-X-MEDIA-SEQUENCE:"));
		}
		else if(strncasecmp(oneLine.Ptr, "#EXTINF:", strlen("#EXTINF:")) == 0)
		{
			fSegmentsNum ++;
			if(fSegmentsNum > MAX_SEGMENT_NUM)
			{
				break;
			}
			fSegments[fSegmentsNum-1].inf = atoi(oneLine.Ptr+strlen("#EXTINF:"));
		}
		else if(strncasecmp(oneLine.Ptr, "#EXT-X-BYTERANGE:", strlen("#EXT-X-BYTERANGE:")) == 0)
		{			
			fSegments[fSegmentsNum-1].byte_range = atol(oneLine.Ptr+strlen("#EXT-X-BYTERANGE:"));
		}
		else if(strncasecmp(oneLine.Ptr, "#EXT-X-PROGRAM-DATE-TIME:", strlen("#EXT-X-PROGRAM-DATE-TIME:")) == 0)
		{			
			// do nothing.
		}
		else if(oneLine.Ptr[0] != '#')
		{			
			if(fSegmentsNum > 0)
			{				
				int len = oneLine.Len;
				if(len >= MAX_URL_LEN)
				{
					fprintf(stderr, "%s: url len[%d] >= MAX_URL_LEN[%d]\n", __PRETTY_FUNCTION__, len, MAX_URL_LEN);
					len = MAX_URL_LEN - 1;
				}
				strncpy(fSegments[fSegmentsNum-1].url, oneLine.Ptr, len);
				fSegments[fSegmentsNum-1].url[len] = '\0';				
				
				StringParser lineParser(&oneLine);
				if(strncasecmp(oneLine.Ptr, "http://", strlen("http://")) == 0)
				{
					lineParser.ConsumeLength(NULL, strlen("http://"));
					// skip host
					lineParser.ConsumeUntil(NULL, '/');
					// get relative_url
					int relative_len = lineParser.GetDataRemaining();
					if(relative_len >= MAX_URL_LEN)
					{
						fprintf(stderr, "%s: relative_len[%d] >= MAX_URL_LEN[%d]\n", __PRETTY_FUNCTION__, relative_len, MAX_URL_LEN);
						relative_len = MAX_URL_LEN - 1;
					}
					strncpy(fSegments[fSegmentsNum-1].relative_url, lineParser.GetCurrentPosition(), relative_len);
					fSegments[fSegmentsNum-1].relative_url[relative_len] = '\0';
					// skip /
					lineParser.Expect('/');
					// skip m3u8 path
					lineParser.ConsumeLength(NULL, fM3U8Path.Len);
					lineParser.Expect('/');
				}
				
				// get m3u8_relative_url
				int m3u8_relative_len = lineParser.GetDataRemaining();
				if(m3u8_relative_len >= MAX_URL_LEN)
				{
					fprintf(stderr, "%s: m3u8_relative_len[%d] >= MAX_URL_LEN[%d]\n", __PRETTY_FUNCTION__, m3u8_relative_len, MAX_URL_LEN);
					m3u8_relative_len = MAX_URL_LEN - 1;
				}
				strncpy(fSegments[fSegmentsNum-1].m3u8_relative_url, lineParser.GetCurrentPosition(), m3u8_relative_len);
				fSegments[fSegmentsNum-1].m3u8_relative_url[m3u8_relative_len] = '\0';

				if(strlen(fSegments[fSegmentsNum-1].relative_url) == 0)
				{
					snprintf(fSegments[fSegmentsNum-1].relative_url, MAX_URL_LEN-1, "/%s/%s", 
						fM3U8Path.Ptr, fSegments[fSegmentsNum-1].m3u8_relative_url);
					fSegments[fSegmentsNum-1].relative_url[MAX_URL_LEN-1] = '\0';
				}

				// get sequence
				lineParser.ConsumeUntil(NULL, '_');
				lineParser.Expect('_');
				lineParser.ConsumeUntil(NULL, '_');
				lineParser.Expect('_');
				lineParser.ConsumeUntil(NULL, '_');
				lineParser.Expect('_');
				lineParser.ConsumeUntil(NULL, '_');
				lineParser.Expect('_');
				fSegments[fSegmentsNum-1].sequence = atol(lineParser.GetCurrentPosition());
			}
		}		
    }
    
	return 0;
}

