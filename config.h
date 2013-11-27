
#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <sys/types.h>
#include <limits.h>

#define MAX_IP_LEN	16

typedef struct config_t
{
	char 		ip[MAX_IP_LEN];	
	u_int16_t	port;
	char		work_path[PATH_MAX];
	char		channels_file[PATH_MAX];
	int32_t		max_clip_num;
} CONFIG_T;

extern CONFIG_T	g_config;

int config_read(CONFIG_T* configp, char* file_name);

#endif
