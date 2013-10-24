//file: HTTPRequest.cpp

#include "BaseServer/StringTranslator.h"

#include "HTTPRequest.h"

UriParam::UriParam(StrPtrLen* keyp, StrPtrLen* valuep)
{
	key = (char*)malloc(keyp->Len + 1);
	memcpy(key, keyp->Ptr, keyp->Len);
	key[keyp->Len] = '\0';

	value = (char*)malloc(valuep->Len + 1);
	memcpy(value, valuep->Ptr, valuep->Len);
	value[valuep->Len] = '\0';
}

UriParam::~UriParam()
{
	if(key != NULL)
	{
		free(key);
		key = NULL;
	}

	if(value != NULL)
	{
		free(value);
		value = NULL;
	}
}

void UriParam_release(void* datap)
{
	if(datap == NULL)
	{
		return;
	}
	UriParam* paramp = (UriParam*)datap;
	delete paramp;
}

UInt8 HTTPRequest::sURLStopConditions[] =
{
  0, 0, 0, 0, 0, 0, 0, 0, 0, 1, //0-9      //'\t' is a stop condition
  1, 0, 0, 1, 0, 0, 0, 0, 0, 0, //10-19    //'\r' & '\n' are stop conditions
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //20-29
  0, 0, 1, 0, 0, 0, 0, 0, 0, 0, //30-39    //' '
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //40-49
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //50-59
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //60-69
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //70-79
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //80-89
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //90-99
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //100-109
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //110-119
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //120-129
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //130-139
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //140-149
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //150-159
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //160-169
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //170-179
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //180-189
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //190-199
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //200-209
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //210-219
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //220-229
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //230-239
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //240-249
  0, 0, 0, 0, 0, 0                         //250-255
};

HTTPRequest::HTTPRequest()
{
	fParamPairs = NULL;
    Clear();
}

HTTPRequest::~HTTPRequest()
{
	deque_release(fParamPairs, UriParam_release);
	fParamPairs = NULL;
}

void HTTPRequest::Clear()
{
    fFullRequest.Set(NULL, 0);    
    fMethod = httpGetMethod;
    fVersion = http10Version;    
    fAbsoluteURI.Set(NULL, 0);    
    fRelativeURI.Set(NULL, 0);    
    fAbsoluteURIScheme.Set(NULL, 0);    
    fHostHeader.Set(NULL, 0);
    fRequestPath = NULL;
    fStatusCode = qtssSuccessOK;
}

QTSS_Error  HTTPRequest::Parse(StrPtrLen* str)
{
    QTSS_Error error;

    fFullRequest.Set(str->Ptr, str->Len);
    
    StringParser parser(str);  

    //parse status line.
    error = this->ParseFirstLine(&parser);
    //handle any errors that come up    
    if (error != QTSS_NoErr)
    {
        return error;
    }
   
#if 0
    error = this->ParseHeaders(parser);
    if (error != QTSS_NoErr)
    {
        return error;
    }
#endif

    return QTSS_NoErr;
};


QTSS_Error HTTPRequest::ParseFirstLine(StringParser* parser)
{   
    // Get the method - If the method is not one of the defined methods
    // then it doesn't return an error but sets fMethod to httpIllegalMethod
    StrPtrLen theParsedData;
    parser->ConsumeWord(&theParsedData);
    fMethod = HTTPProtocol::GetMethod(&theParsedData);
    
    // Consume whitespace
    parser->ConsumeWhitespace();
  
    // Parse the URI - If it fails returns an error after setting 
    // the fStatusCode to the appropriate error code
    QTSS_Error err = ParseURI(parser);
    if (err != QTSS_NoErr)
            return err;
  
    // Consume whitespace
    parser->ConsumeWhitespace();
  
    // If there is a version, consume the version string
    StrPtrLen versionStr;
    parser->ConsumeUntil(&versionStr, StringParser::sEOLMask);
    // Check the version
    if (versionStr.Len > 0)
            fVersion = HTTPProtocol::GetVersion(&versionStr);
  
    // Go past the end of line
    if (!parser->ExpectEOL())
        {
            fStatusCode = httpBadRequest;
            return QTSS_BadArgument;     // Request line is not properly formatted!
        }

    return QTSS_NoErr;
}


QTSS_Error HTTPRequest::ParseURI(StringParser* parser)
{

    // read in the complete URL into fRequestAbsURI
    parser->ConsumeUntil(&fAbsoluteURI, sURLStopConditions);
  
    StringParser urlParser(&fAbsoluteURI);
  
    // we always should have a slash before the URI
    // If not, that indicates this is a full URI
    if (fAbsoluteURI.Ptr[0] != '/')
    {
            //if it is a full URL, store the scheme and host name
            urlParser.ConsumeLength(&fAbsoluteURIScheme, 7); //consume "http://"
            urlParser.ConsumeUntil(&fHostHeader, '/');
    }
  
    // whatever is in this position is the relative URI
    StrPtrLen relativeURI(urlParser.GetCurrentPosition(), urlParser.GetDataReceivedLen() - urlParser.GetDataParsedLen());
    // read this URI into fRequestRelURI
    fRelativeURI = relativeURI;

    urlParser.ConsumeUntil(&fURIPath, '?');
    
    // Allocate memory for fRequestPath
    UInt32 len = fRelativeURI.Len;
    #if 1
    
    len++;    
    char* relativeURIDecoded = new char[len];
    
    SInt32 theBytesWritten = StringTranslator::DecodeURL(fRelativeURI.Ptr, fRelativeURI.Len,
                                                       relativeURIDecoded, len);
     
    //if negative, an error occurred, reported as an QTSS_Error
    //we also need to leave room for a terminator.
    if ((theBytesWritten < 0) || ((UInt32)theBytesWritten == len))
        {
            fStatusCode = httpBadRequest;
            return QTSS_BadArgument;
        }
    fRequestPath = new char[theBytesWritten + 1];
    ::memcpy(fRequestPath, relativeURIDecoded + 1, theBytesWritten); 
    delete relativeURIDecoded;
    
    #else
    
    fRequestPath = new char[len+1];
    ::memcpy(fRequestPath, fRelativeURI.Ptr, len); 
    fRequestPath[len] = '\0';
    
    #endif

	{
		StrPtrLen request_path(fRequestPath);
		StringParser urlParser(&request_path);
		urlParser.ConsumeUntil(&fURIPath, '?');
		
	    StrPtrLen UriParams(urlParser.GetCurrentPosition(), urlParser.GetDataReceivedLen() - urlParser.GetDataParsedLen());
	    fURIParams = UriParams;

		if(fURIParams.Len > 0)
		{
	    	ParseParams(&urlParser);    
	    }
    }
    
    return QTSS_NoErr;
}

QTSS_Error HTTPRequest::ParseParams(StringParser* parser)
{
	Bool16 ret = parser->Expect('?');
	if(!ret)
	{
		return QTSS_BadArgument;
	}

	while(1)
	{
		StrPtrLen param;
		parser->ConsumeUntil(&param, '&');
		if(param.Len == 0)
		{
			break;
		}
		ParseParam(&param);
		parser->Expect('&');		
	}
	
	
	return QTSS_NoErr;
}

QTSS_Error HTTPRequest::ParseParam(StrPtrLen* param)
{
	StringParser parser(param);

	StrPtrLen key;
	parser.ConsumeUntil(&key, '=');
	parser.Expect('=');	

	StrPtrLen value(parser.GetCurrentPosition(), parser.GetDataReceivedLen()-parser.GetDataParsedLen());	

	UriParam* paramp = new UriParam(&key, &value);
	if(paramp == NULL)
	{
		return QTSS_RequestFailed;
	}

	
	DEQUE_NODE* nodep = (DEQUE_NODE*)malloc(sizeof(DEQUE_NODE));
	if(nodep == NULL)
	{
		delete paramp;
		paramp = NULL;
		return QTSS_RequestFailed;
	}

	memset(nodep, 0, sizeof(DEQUE_NODE));
	nodep->datap = paramp;
	fParamPairs = deque_append(fParamPairs, nodep);	

	return QTSS_NoErr;
}


