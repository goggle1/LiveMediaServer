//file: HTTPRequest.h

#ifndef __HTTPREQUEST_H__
#define __HTTPREQUEST_H__

#include "BaseServer/OSHeaders.h"
#include "BaseServer/StringParser.h"

#include "HTTPProtocol.h"

#include "deque.h"

enum
{
    QTSS_NoErr              = 0,
    QTSS_RequestFailed      = -1,
    QTSS_Unimplemented      = -2,
    QTSS_RequestArrived     = -3,
    QTSS_OutOfState         = -4,
    QTSS_NotAModule         = -5,
    QTSS_WrongVersion       = -6,
    QTSS_IllegalService     = -7,
    QTSS_BadIndex           = -8,
    QTSS_ValueNotFound      = -9,
    QTSS_BadArgument        = -10,
    QTSS_ReadOnly           = -11,
    QTSS_NotPreemptiveSafe  = -12,
    QTSS_NotEnoughSpace     = -13,
    QTSS_WouldBlock         = -14,
    QTSS_NotConnected       = -15,
    QTSS_FileNotFound       = -16,
    QTSS_NoMoreData         = -17,
    QTSS_AttrDoesntExist    = -18,
    QTSS_AttrNameExists     = -19,
    QTSS_InstanceAttrsNotAllowed= -20
};
typedef SInt32 QTSS_Error;

#define QTSS_ResponseDone	QTSS_RequestArrived

enum
{
    qtssContinue                        = 0,        //100
    qtssSuccessOK                       = 1,        //200
    qtssSuccessCreated                  = 2,        //201
    qtssSuccessAccepted                 = 3,        //202
    qtssSuccessNoContent                = 4,        //203
    qtssSuccessPartialContent           = 5,        //204
    qtssSuccessLowOnStorage             = 6,        //250
    qtssMultipleChoices                 = 7,        //300
    qtssRedirectPermMoved               = 8,        //301
    qtssRedirectTempMoved               = 9,        //302
    qtssRedirectSeeOther                = 10,       //303
    qtssRedirectNotModified             = 11,       //304
    qtssUseProxy                        = 12,       //305
    qtssClientBadRequest                = 13,       //400
    qtssClientUnAuthorized              = 14,       //401
    qtssPaymentRequired                 = 15,       //402
    qtssClientForbidden                 = 16,       //403
    qtssClientNotFound                  = 17,       //404
    qtssClientMethodNotAllowed          = 18,       //405
    qtssNotAcceptable                   = 19,       //406
    qtssProxyAuthenticationRequired     = 20,       //407
    qtssRequestTimeout                  = 21,       //408
    qtssClientConflict                  = 22,       //409
    qtssGone                            = 23,       //410
    qtssLengthRequired                  = 24,       //411
    qtssPreconditionFailed              = 25,       //412
    qtssRequestEntityTooLarge           = 26,       //413
    qtssRequestURITooLarge              = 27,       //414
    qtssUnsupportedMediaType            = 28,       //415
    qtssClientParameterNotUnderstood    = 29,       //451
    qtssClientConferenceNotFound        = 30,       //452
    qtssClientNotEnoughBandwidth        = 31,       //453
    qtssClientSessionNotFound           = 32,       //454
    qtssClientMethodNotValidInState     = 33,       //455
    qtssClientHeaderFieldNotValid       = 34,       //456
    qtssClientInvalidRange              = 35,       //457
    qtssClientReadOnlyParameter         = 36,       //458
    qtssClientAggregateOptionNotAllowed = 37,       //459
    qtssClientAggregateOptionAllowed    = 38,       //460
    qtssClientUnsupportedTransport      = 39,       //461
    qtssClientDestinationUnreachable    = 40,       //462
    qtssServerInternal                  = 41,       //500
    qtssServerNotImplemented            = 42,       //501
    qtssServerBadGateway                = 43,       //502
    qtssServerUnavailable               = 44,       //503
    qtssServerGatewayTimeout            = 45,       //505
    qtssRTSPVersionNotSupported         = 46,       //504
    qtssServerOptionNotSupported        = 47,       //551
    qtssNumStatusCodes                  = 48
    
};
typedef UInt32 QTSS_RTSPStatusCode;


class UriParam
{
	public:
		UriParam(StrPtrLen* keyp, StrPtrLen* valuep);
		~UriParam();
	public:
		char* key;
		char* value;
};

class HTTPRequest
{
public:   
    HTTPRequest();
    virtual ~HTTPRequest() ;
    QTSS_Error  Parse(StrPtrLen* str); 
    void        Clear();
    
public:
    QTSS_Error  ParseFirstLine(StringParser* parser);
    QTSS_Error  ParseURI(StringParser* parser);
    QTSS_Error  ParseParams(StringParser* parser);
    QTSS_Error  ParseParam(StrPtrLen* param);
    // Parses the headers and adds them into a dictionary
    // Also calls SetKeepAlive with the Connection header field's value if it exists
    QTSS_Error              ParseHeaders(StringParser* parser);
    // Sets fRequestKeepAlive
    void                    SetKeepAlive(StrPtrLen* keepAliveValue);
    
    StrPtrLen                   fFullRequest;
    
    HTTPMethod                  fMethod;            //Method of this request 
    HTTPVersion                 fVersion;

    // For the URI (fAbsoluteURI and fRelativeURI are the same if the URI is of the form "/path")
    StrPtrLen           fAbsoluteURI;       // If it is of the form "http://foo.bar.com/path?params"
    StrPtrLen           fRelativeURI;       // If it is of the form "/path?params"

	StrPtrLen           fURIPath;       	// If it is of the form "/path"
    StrPtrLen			fURIParams;			// ?key1=value&key2=value2
    DEQUE_NODE*			fParamPairs;		// key:value, key:value(UriParam) 
    
                                            
                                            // If it is an absolute URI, these fields will be filled in
                                            // "http://foo.bar.com/path" => fAbsoluteURIScheme = "http", fHostHeader = "foo.bar.com",
                                            // fRequestPath = "path"
    StrPtrLen           fAbsoluteURIScheme;
    StrPtrLen           fHostHeader;        // If the full url is given in the request line
    char*               fRequestPath;       // Also contains the query string
    
    //for Error Response
    HTTPStatusCode		fStatusCode;
    Bool16              fRequestKeepAlive;              // Keep-alive information in the client request
    StrPtrLen           fFieldValues[httpNumHeaders];   // Array of header field values parsed from the request
    StrPtrLen           fSvrHeader;                     // Server header set up at initialization
    static StrPtrLen    sColonSpace;
    static UInt8        sURLStopConditions[]; 
};

#endif
