
#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <sys/types.h>
#include <limits.h>

#define MAX_IP_LEN		16
#define MIN_CLIP_NUM 	3
// 10 ms
#define MIN_DOWNLOAD_INTERVAL 		10
// 5000 ms
#define DEFAULT_DOWNLOAD_INTERVAL	5000
// 1 kpbs
#define MIN_DOWNLOAD_LIMIT 			1000
#define DEFAULT_CHANNELS_FILE		"channels.xml"

typedef struct config_t
{
	char 		ip[MAX_IP_LEN];	
	u_int16_t	port;
	char 		service_ip[MAX_IP_LEN];
	char		etc_path[PATH_MAX];
	char		bin_path[PATH_MAX];
	char		log_path[PATH_MAX];
	char		html_path[PATH_MAX];
	//char		work_path[PATH_MAX];
	char		channels_fullpath[PATH_MAX];
	int32_t		max_clip_num;
	int32_t		download_interval;
	//bps
	int64_t		download_limit;
} CONFIG_T;

extern CONFIG_T	g_config;
extern char*	g_config_file;

int config_read(CONFIG_T* configp, char* file_name);
int config_write(CONFIG_T* configp, char* file_name);

#endif
