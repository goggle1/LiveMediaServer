
#ifndef __HTTPRESPONSE_H__
#define __HTTPRESPONSE_H__

class HTTPResponse
{
	public:
		HTTPResponse();
		~HTTPResponse();
		int parse(char* data, int len);
	public:
		int fContentLength;
};

#endif
