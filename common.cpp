#include <sys/syscall.h>
#include <unistd.h>
#include <sys/time.h>

#include "common.h"

int gettid()
{
	return syscall(SYS_gettid);
}

int timeval_cmp(struct timeval* t1, struct timeval* t2)
{
	if(t1->tv_sec < t2->tv_sec)
	{
		return -1;
	}
	else if(t1->tv_sec > t2->tv_sec)
	{
		return 1;
	}
	else // if(t1->tv_sec == t2->tv_sec)
	{
		if(t1->tv_usec < t2->tv_usec)
		{
			return -1;
		}
		else if(t1->tv_usec > t2->tv_usec)
		{
			return 1;
		}
		else // if(t1->tv_usec == t2->tv_usec)
		{
			return 0;
		}
	}
}


time_t timeval_diff(struct timeval* t2, struct timeval* t1)
{
	time_t ret1 = 0;
	time_t ret2 = 0;
	ret1 = t2->tv_sec - t1->tv_sec;
	ret2 = ret1 * 1000 + (t2->tv_usec - t1->tv_usec)/1000;
	return ret2;
}


