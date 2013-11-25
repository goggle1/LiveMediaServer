
#include "../BaseServer/StrPtrLen.h"
#include "../BaseServer/StringParser.h"

#include "HTTPResponse.h"

HTTPResponse::HTTPResponse()
{
	fContentLength = 0;
}

HTTPResponse::~HTTPResponse()
{
}

int HTTPResponse::parse(char* data, int len)
{
	StrPtrLen response_header(data, len);
	StringParser parser(&response_header);

	while(1)
	{
		StrPtrLen line;
		parser.ConsumeEOL(&line);
		if(line.Len<=0)
		{
			break;
		}
		parser.ExpectEOL();

		StringParser line_parser(&line);
		StrPtrLen key;
		line_parser.ConsumeUntil(&key, ':');
		line_parser.Expect(':');
		line_parser.Expect(' ');		
		if(strcmp(key.Ptr, "Content-Length") == 0)
		{
			fContentLength = atoi(line_parser.GetCurrentPosition());
		}
		// ...
	}

	return 0;
}
