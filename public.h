
#ifndef __PUBLIC_H__
#define __PUBLIC_H__

#define PROGRAM_VERSION		"0.1.0.018"
#define MY_VERSION			PROGRAM_VERSION"@"OS_VERSION
#define CMD_VERSION			"2.1"

#define BASE_SERVER_NAME 	"TeslaStreamingServer"
#define BASE_SERVER_VERSION MY_VERSION

#define USER_AGENT			"LiveMediaServer"

#define ROOT_PATH			"/home/html"
#define MAX_CLIP_NUM		60
//#define MAX_CLIP_NUM		61

#define MAX_URL_LEN			256

// 10 seconds
//#define MAX_SEMENT_TIME		10000
#define MAX_SEMENT_TIME		5000
#define MAX_TIMEOUT_TIME	(10000*2)

#define MAX_HOST_LEN		64

#define URI_LIVESTREAM		"/livestream/"

#define DEFAULT_SEGMENT_NUM	3


#endif

