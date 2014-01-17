
#ifndef __M3U8PARSER_H__
#define __M3U8PARSER_H__

#include <sys/types.h>

#include "BaseServer/StrPtrLen.h"
#include "public.h"

//#define MAX_SEGMENT_NUM	3
#define MAX_SEGMENT_NUM	24


typedef struct segment_t
{
	//#EXTINF:10,
	u_int32_t 	inf;
	//#EXT-X-BYTERANGE:1095852
	u_int64_t	byte_range;
	// 20131024_155801
	time_t		begin_time;
	// 565631
	u_int64_t	sequence;	
	// http://lm.funshion.com/livestream/3702892333/fd5f6b86b836e38c8eed27c9e66e3e6dcf0a69b2/ts/2013/10/25/20131017T174027_03_20131024_155801_565631.ts
	// /livestream/3702892333/fd5f6b86b836e38c8eed27c9e66e3e6dcf0a69b2/ts/2013/10/25/20131017T174027_03_20131024_155801_565631.ts
	char 	url[MAX_URL_LEN];	
	// /livestream/3702892333/fd5f6b86b836e38c8eed27c9e66e3e6dcf0a69b2/ts/2013/10/25/20131017T174027_03_20131024_155801_565631.ts
	char	relative_url[MAX_URL_LEN];
	// 3702892333/fd5f6b86b836e38c8eed27c9e66e3e6dcf0a69b2/ts/2013/10/25/20131017T174027_03_20131024_155801_565631.ts
	char	m3u8_relative_url[MAX_URL_LEN];
	// 20131017T174027_03_20131024_155801_565631.ts
	char 	file_name[MAX_URL_LEN];
} SEGMENT_T;

class M3U8Parser
{
	public:
		M3U8Parser();
		~M3U8Parser();
		int		SetPath(StrPtrLen* pathp);
		int    	Parse(char* datap, UInt32 len);
		time_t	GetNewestTime();
		Bool16	IsOld();

	public:
		StrPtrLen   fData;
		StrPtrLen	fM3U8Path;

		// parse result:
		int 		fTargetDuration;
		u_int64_t	fMediaSequence;
		int			fSegmentsNum;
		SEGMENT_T	fSegments[MAX_SEGMENT_NUM];
};

#endif
