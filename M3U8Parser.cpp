#include <stdio.h>
#include <stdlib.h>

#include "BaseServer/StringParser.h"

#include "M3U8Parser.h"

M3U8Parser::M3U8Parser()
{
}


M3U8Parser::~M3U8Parser()
{
}

int M3U8Parser::Parse(char * datap, UInt32 len)
{
	if(fData.Ptr != NULL)
	{
		delete []fData.Ptr;
		fData.Ptr = NULL;
		fData.Len = 0;
	}
	
	char *dataCopy = new char[len];  
    memcpy(dataCopy, datap, len);
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

	int segment_num = 0;
	StrPtrLen oneLine;
	StringParser dataParser(&fData);		
	while (dataParser.GetDataRemaining() > 0)
	{
		dataParser.GetThruEOL(&oneLine);
		if (oneLine.Len == 0)
			continue;//skip over any blank lines
	
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
			fMediaSequence = atoi(oneLine.Ptr+strlen("#EXT-X-MEDIA-SEQUENCE:"));
		}
		else if(strncasecmp(oneLine.Ptr, "#EXTINF:", strlen("#EXTINF:")) == 0)
		{
			segment_num ++;
			fSegments[segment_num-1].inf = atoi(oneLine.Ptr+strlen("#EXTINF:"));
		}
		else if(strncasecmp(oneLine.Ptr, "#EXT-X-BYTERANGE:", strlen("#EXT-X-BYTERANGE:")) == 0)
		{			
			fSegments[segment_num-1].byte_range = atoi(oneLine.Ptr+strlen("#EXT-X-BYTERANGE:"));
		}
		else 
		{			
			if(segment_num > 0)
			{
				int len = oneLine.Len;
				if(len < MAX_URL_LEN)
				{
					strncpy(fSegments[segment_num-1].url, oneLine.Ptr, len);
					fSegments[segment_num-1].url[len] = '\0';
				}
				else
				{
					fprintf(stderr, "%s: url len[%d] > MAX_URL_LEN[%d]\n", __PRETTY_FUNCTION__, len, MAX_URL_LEN);
					strncpy(fSegments[segment_num-1].url, oneLine.Ptr, MAX_URL_LEN-1);
					fSegments[segment_num-1].url[MAX_URL_LEN-1] = '\0';
				}
			}
		}		
    }
    
	return 0;
}

