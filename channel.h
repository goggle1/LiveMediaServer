
#ifndef _CHANNEL_H__
#define _CHANNEL_H__

#include <libxml/xmlmemory.h>
#include <libxml/parser.h>

#include "deque.h"

#define HASH_LEN 			40
#define MAX_LIVE_ID     	44
#define MAX_CHANNEL_NAME	64

typedef struct source_t
{
	u_int32_t	ip;
	u_int16_t	port;
} SOURCE_T;

//{"channel_id":"21","LOWER(t.liveid)":"0b49884b3b85f7ccddbe4e96e4ae2eae7a6dec56","bitrate":"800","channel_name":"\u4e1c\u65b9\u536b\u89c6"},
typedef struct channel_t
{
	int 	channel_id;	
	char 	liveid[MAX_LIVE_ID];
	int 	bitrate;
	char	channel_name[MAX_CHANNEL_NAME];
	int	  	codec_ts; 	// 1: on, 0: off
	int		codec_flv;	// 1: on, 0: off
	int		codec_mp4;	// 1: on, 0: off
	DEQUE_NODE* source_list;
} CHANNEL_T;

class ChannelList
{
	public:
		ChannelList();
		~ChannelList();

		int			ReadConfig(char* config_file);
		int			WriteConfig(char* config_file);

		CHANNEL_T* 	FindChannel(char* liveid);
		int			AddChannel(CHANNEL_T* channelp);
		int			DeleteChannel(char* liveid);
		DEQUE_NODE* GetChannels() { return m_channel_list; };
		
	protected:
		DEQUE_NODE* ParseChannels(xmlDocPtr doc, xmlNodePtr cur);
		int			ParseChannel(xmlDocPtr doc, xmlNodePtr cur, CHANNEL_T* channelp);
		DEQUE_NODE* ParseSources(xmlDocPtr doc, xmlNodePtr cur);
		int			ParseSource(xmlDocPtr doc, xmlNodePtr cur, SOURCE_T* sourcep);
		
		DEQUE_NODE*	m_channel_list;		
};

extern ChannelList g_channels;

#endif
