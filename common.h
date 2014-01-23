
#ifndef __COMMON_H__
#define __COMMON_H__

#define PROGRAM_VERSION		"0.1.0.026"
#define MY_VERSION			PROGRAM_VERSION"@"OS_VERSION
#define CMD_VERSION			"2.1"

#define BASE_SERVER_NAME 	"TeslaStreamingServer"
#define BASE_SERVER_VERSION MY_VERSION

#define USER_AGENT			"LiveMediaServer"

#define ROOT_PATH			"/home/html"
#define MAX_CLIP_NUM		60
//#define MAX_CLIP_NUM		61

#define MAX_URL_LEN			256
#define MAX_HOST_LEN		64
#define MAX_TIME_LEN		64

#define URI_LIVESTREAM		"/livestream/"

#define DEFAULT_SEGMENT_NUM	3
//#define MAX_SEGMENT_NUM	3
#define MAX_SEGMENT_NUM		24

// 10 seconds
//#define SEMENT_DURATION		10000
//#define MAX_DOWNLOAD_TIME	(g_config.clip_duration*1000*2)
#define MAX_DOWNLOAD_TIME	(g_config.clip_duration*1000)


#define MAX_CONNECT_TIME	5000


extern struct timeval		g_start_time;

int gettid();
int timeval_cmp(struct timeval* t1, struct timeval* t2);
time_t timeval_diff(struct timeval* t2, struct timeval* t1);


#endif

