#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "channel.h"
#include "HTTPClientSession.h"

void source_release(void* datap)
{
	if(datap == NULL)
	{
		return;
	}

	SOURCE_T* sourcep = (SOURCE_T*)datap;
	// do nothing.
	
	free(sourcep);
}


void channel_release(void* datap)
{
	if(datap == NULL)
	{
		return;
	}
	
	CHANNEL_T* channelp = (CHANNEL_T*)datap;
	if(channelp->source_list != NULL)
	{
		deque_release(channelp->source_list, source_release);	
	}
	
	free(channelp);
}

DEQUE_NODE* channel_find_source(CHANNEL_T* channelp, u_int32_t ip)
{
	DEQUE_NODE* nodep = channelp->source_list;
	while(nodep)
	{
		SOURCE_T* sourcep = (SOURCE_T*)nodep->datap;
		if(sourcep->ip == ip)
		{
			return nodep;
		}
		
		if(nodep->nextp == channelp->source_list)
		{
			break;
		}
		nodep = nodep->nextp;		
	}

	return NULL;
}

int channel_add_source(CHANNEL_T* channelp, u_int32_t ip, u_int16_t port)
{
	DEQUE_NODE* nodep = (DEQUE_NODE*)malloc(sizeof(DEQUE_NODE));
	if(nodep == NULL)
	{
		return -1;
	}
	memset(nodep, 0, sizeof(DEQUE_NODE));
	
	SOURCE_T* sourcep = (SOURCE_T*)malloc(sizeof(SOURCE_T));
	if(sourcep == NULL)
	{
		free(nodep);
		return -1;
	}
	sourcep->ip = ip;
	sourcep->port = port;

	nodep->datap = sourcep;

	channelp->source_list = deque_append(channelp->source_list, nodep);
	
	return 0;
}


int start_channel(CHANNEL_T* channelp)
{
	DEQUE_NODE* source_list = channelp->source_list;
	DEQUE_NODE* nodep = source_list;
	if(nodep == NULL)
	{
		return -1;
	}
	
	SOURCE_T* sourcep = (SOURCE_T*)nodep->datap;
	if(channelp->codec_ts)
	{		
		//StrPtrLen 	inURL("http://192.168.8.197:1180/1100000000000000000000000000000000000000.m3u8");
		//StrPtrLen 	inURL("http://lv.funshion.com/livestream/fd5f6b86b836e38c8eed27c9e66e3e6dcf0a69b2.m3u8?codec=ts");
		char* type = "ts";
		char url[MAX_URL_LEN];
		snprintf(url, MAX_URL_LEN-1, "/livestream/%s.m3u8?codec=%s", channelp->liveid, type);
		url[MAX_URL_LEN-1] = '\0';
		StrPtrLen inURL(url);
		HTTPClientSession* sessionp = new HTTPClientSession(sourcep->ip, sourcep->port, inURL, channelp->liveid, type);	
		if(sessionp == NULL)
		{
			return -1;
		}
		channelp->sessionp_ts = sessionp;
	}
	if(channelp->codec_flv)
	{		
		char* type = "flv";
		char url[MAX_URL_LEN];
		snprintf(url, MAX_URL_LEN-1, "/livestream/%s.m3u8?codec=%s", channelp->liveid, type);
		url[MAX_URL_LEN-1] = '\0';
		StrPtrLen inURL(url);
		HTTPClientSession* sessionp = new HTTPClientSession(sourcep->ip, sourcep->port, inURL, channelp->liveid, type);	
		if(sessionp == NULL)
		{
			return -1;
		}
		channelp->sessionp_flv = sessionp;
	}
	if(channelp->codec_mp4)
	{		
		char* type = "mp4";
		char url[MAX_URL_LEN];
		snprintf(url, MAX_URL_LEN-1, "/livestream/%s.m3u8?codec=%s", channelp->liveid, type);
		url[MAX_URL_LEN-1] = '\0';
		StrPtrLen inURL(url);
		HTTPClientSession* sessionp = new HTTPClientSession(sourcep->ip, sourcep->port, inURL, channelp->liveid, type);	
		if(sessionp == NULL)
		{
			return -1;
		}
		channelp->sessionp_mp4 = sessionp;
	}	
	
	return 0;
}

int stop_channel(CHANNEL_T* channelp)
{
	if(channelp->sessionp_ts != NULL)
	{	
		channelp->sessionp_ts->Signal(Task::kKillEvent);
		channelp->sessionp_ts = NULL;
	}

	if(channelp->sessionp_flv != NULL)
	{	
		channelp->sessionp_flv->Signal(Task::kKillEvent);
		channelp->sessionp_flv = NULL;
	}

	if(channelp->sessionp_mp4 != NULL)
	{	
		channelp->sessionp_mp4->Signal(Task::kKillEvent);
		channelp->sessionp_mp4 = NULL;
	}
	
	
	return 0;
}


ChannelList::ChannelList()
{
	m_channel_list = NULL;
}

ChannelList::~ChannelList()
{
	if(m_channel_list != NULL)
	{
		deque_release(m_channel_list, channel_release);
		m_channel_list = NULL;
	}
}

int ChannelList::ParseChannel(xmlDocPtr doc, xmlNodePtr cur, CHANNEL_T* channelp)
{	
	xmlAttrPtr attrPtr = cur->properties;
	while (attrPtr != NULL)
	{
		if (!xmlStrcmp(attrPtr->name, BAD_CAST "channel_id"))
		{
			xmlChar* szAttr = xmlGetProp(cur, BAD_CAST "channel_id");		   
			channelp->channel_id = atoi((const char*)szAttr);
			xmlFree(szAttr);
		}
		else if (!xmlStrcmp(attrPtr->name, BAD_CAST "liveid"))
		{
			xmlChar* szAttr = xmlGetProp(cur, BAD_CAST "liveid");		   
			strncpy(channelp->liveid, (const char*)szAttr, MAX_LIVE_ID-1);
			channelp->liveid[MAX_LIVE_ID-1] = '\0';
			xmlFree(szAttr);
		}
		else if (!xmlStrcmp(attrPtr->name, BAD_CAST "bitrate"))
		{
			xmlChar* szAttr = xmlGetProp(cur, BAD_CAST "bitrate");		   
			channelp->bitrate = atoi((const char*)szAttr);
			xmlFree(szAttr);
		}
		else if (!xmlStrcmp(attrPtr->name, BAD_CAST "channel_name"))
		{
			xmlChar* szAttr = xmlGetProp(cur, BAD_CAST "channel_name");		   
			strncpy(channelp->channel_name, (const char*)szAttr, MAX_CHANNEL_NAME-1);
			channelp->channel_name[MAX_CHANNEL_NAME-1] = '\0';
			xmlFree(szAttr);
		}
		else if (!xmlStrcmp(attrPtr->name, BAD_CAST "codec_ts"))
		{
			xmlChar* szAttr = xmlGetProp(cur, BAD_CAST "codec_ts");		   
			channelp->codec_ts = atoi((const char*)szAttr);
			xmlFree(szAttr);
		}
		else if (!xmlStrcmp(attrPtr->name, BAD_CAST "codec_flv"))
		{
			xmlChar* szAttr = xmlGetProp(cur, BAD_CAST "codec_flv");		   
			channelp->codec_flv = atoi((const char*)szAttr);
			xmlFree(szAttr);
		}
		else if (!xmlStrcmp(attrPtr->name, BAD_CAST "codec_mp4"))
		{
			xmlChar* szAttr = xmlGetProp(cur, BAD_CAST "codec_mp4");		   
			channelp->codec_mp4 = atoi((const char*)szAttr);
			xmlFree(szAttr);
		}

		attrPtr = attrPtr->next;
	}

	DEQUE_NODE* source_list = NULL;	
	xmlNodePtr child;	
	child = cur->xmlChildrenNode;
	while((child != NULL) && (child->name != NULL))
	{
		if((!xmlStrcmp(child->name, (const xmlChar*)"sources")))
		{			
			source_list = ParseSources(doc, child);
			if(source_list != NULL)
			{
				channelp->source_list = deque_link(channelp->source_list, source_list);
			}
		}
		
		child = child->next;
	}
	

	return 0;
}

DEQUE_NODE* ChannelList::ParseChannels(xmlDocPtr doc, xmlNodePtr cur)
{	
	DEQUE_NODE* headp = NULL;
	
	xmlNodePtr child;	
	child = cur->xmlChildrenNode;
	while((child != NULL) && (child->name != NULL))
	{
		if((!xmlStrcmp(child->name, (const xmlChar*)"channel")))
		{
			DEQUE_NODE* nodep = (DEQUE_NODE*)malloc(sizeof(DEQUE_NODE));
			if(nodep == NULL)
			{
			    fprintf(stderr, "%s: can not malloc for DEQUE_NODE\n", __FUNCTION__);
			    return headp;
			}
			memset(nodep, 0, sizeof(DEQUE_NODE));
			
			CHANNEL_T* channelp = NULL;
			channelp = (CHANNEL_T*)malloc(sizeof(CHANNEL_T));
			if(channelp == NULL)
			{
			    fprintf(stderr, "%s: can not malloc for CHANNEL_T\n", __FUNCTION__);
			    free(nodep);
			    return headp;
			}
			memset(channelp, 0, sizeof(CHANNEL_T));
			nodep->datap = channelp;
			nodep->prevp = NULL;
			nodep->nextp = NULL;			
			
			int ret = ParseChannel(doc, child, channelp);
			if(ret == 0)
			{		
            	headp = deque_append(headp, nodep);
            }
            else
            {
		    	free(channelp);
            	free(nodep);
            }
		}
		
		child = child->next;
	}
	
	return headp;
}


DEQUE_NODE* ChannelList::ParseSources(xmlDocPtr doc, xmlNodePtr cur)
{	
	DEQUE_NODE* headp = NULL;
	
	xmlNodePtr child;	
	child = cur->xmlChildrenNode;
	while((child != NULL) && (child->name != NULL))
	{
		if((!xmlStrcmp(child->name, (const xmlChar*)"source")))
		{
			DEQUE_NODE* nodep = (DEQUE_NODE*)malloc(sizeof(DEQUE_NODE));
			if(nodep == NULL)
			{
			    fprintf(stderr, "%s: can not malloc for DEQUE_NODE\n", __FUNCTION__);
			    return headp;
			}
			memset(nodep, 0, sizeof(DEQUE_NODE));
			
			SOURCE_T* sourcep = NULL;
			sourcep = (SOURCE_T*)malloc(sizeof(SOURCE_T));
			if(sourcep == NULL)
			{
			    fprintf(stderr, "%s: can not malloc for SOURCE_T\n", __FUNCTION__);
			    free(nodep);
			    return headp;
			}
			memset(sourcep, 0, sizeof(SOURCE_T));
			nodep->datap = sourcep;
			nodep->prevp = NULL;
			nodep->nextp = NULL;			
			
			int ret = ParseSource(doc, child, sourcep);
			if(ret == 0)
			{		
            	headp = deque_append(headp, nodep);
            }
            else
            {
		    	free(sourcep);
            	free(nodep);
            }
		}
		
		child = child->next;
	}
	
	return headp;
}

int ChannelList::ParseSource(xmlDocPtr doc, xmlNodePtr cur, SOURCE_T* sourcep)
{	
	xmlAttrPtr attrPtr = cur->properties;
	while (attrPtr != NULL)
	{
		if (!xmlStrcmp(attrPtr->name, BAD_CAST "ip"))
		{
			xmlChar* szAttr = xmlGetProp(cur, BAD_CAST "ip");		   
			sourcep->ip = inet_network((const char*)szAttr);
			xmlFree(szAttr);
		}
		else if (!xmlStrcmp(attrPtr->name, BAD_CAST "port"))
		{
			xmlChar* szAttr = xmlGetProp(cur, BAD_CAST "port");		   
			sourcep->port = atoi((const char*)szAttr);
			xmlFree(szAttr);
		}

		attrPtr = attrPtr->next;
	}

	return 0;
}

int ChannelList::ReadConfig(char* config_file)
{
	xmlDocPtr doc;
	xmlNodePtr cur;
	
	doc=xmlParseFile(config_file);
	//doc = xmlParseMemory(xml_str.c_str(), xml_str.length());
	if(doc == NULL)
	{		
		fprintf(stderr, "%s: xml parsed failed!\n", __FUNCTION__);
		return -1;
	}

	cur = xmlDocGetRootElement(doc);
	if(cur == NULL)
	{
		fprintf(stderr, "%s: empty document!\n", __FUNCTION__);
		xmlFreeDoc(doc);
		return -1;
	}

	if (xmlStrcmp(cur->name, (const xmlChar *)"channels") != 0)
	{	
		fprintf(stderr, "%s: root node is not 'channels'!\n", __FUNCTION__);
		xmlFreeDoc(doc);
		return -1;
	}
	
	DEQUE_NODE* channel_list = ParseChannels(doc, cur);	
	xmlFreeDoc(doc);

    if(channel_list)
    {
	    m_channel_list = deque_link(m_channel_list, channel_list);
	    return 0;
    }

	
	return 0;
}

#if 0
int ChannelList::WriteConfig(char* config_file)
{
	if(m_channel_list == NULL)
	{
		return -1;
	}

	xmlDocPtr doc = NULL;
	xmlNodePtr channels_node = NULL;
	xmlNodePtr channel_node = NULL;
	xmlNodePtr sources_node = NULL;
	xmlNodePtr source_node = NULL;
	
	doc = xmlNewDoc(BAD_CAST "1.0");
	channels_node = xmlNewNode(NULL, BAD_CAST "channels");
	xmlDocSetRootElement(doc, channels_node);

	DEQUE_NODE* nodep = m_channel_list;
	while(nodep)
	{
		CHANNEL_T* channelp = (CHANNEL_T*)nodep->datap;
		channel_node = xmlNewChild(channels_node, NULL, BAD_CAST "channel", BAD_CAST"");

		char str_channel_id[64] = {'\0'};
		sprintf(str_channel_id, "%d", channelp->channel_id);
		xmlNewProp(channel_node, BAD_CAST "channel_id", BAD_CAST str_channel_id);
		xmlNewProp(channel_node, BAD_CAST "liveid", BAD_CAST channelp->liveid);
		char str_bitrate[64] = {'\0'};
		sprintf(str_bitrate, "%d", channelp->bitrate);
		xmlNewProp(channel_node, BAD_CAST "bitrate", BAD_CAST str_bitrate);
		xmlNewProp(channel_node, BAD_CAST "channel_name", BAD_CAST channelp->channel_name);
		char str_codec[64] = {'\0'};
		sprintf(str_codec, "%d", channelp->codec_ts);
		xmlNewProp(channel_node, BAD_CAST "codec_ts", BAD_CAST str_codec);
		sprintf(str_codec, "%d", channelp->codec_flv);
		xmlNewProp(channel_node, BAD_CAST "codec_flv", BAD_CAST str_codec);
		sprintf(str_codec, "%d", channelp->codec_mp4);
		xmlNewProp(channel_node, BAD_CAST "codec_mp4", BAD_CAST str_codec);

		sources_node = xmlNewChild(channel_node, NULL, BAD_CAST "sources", BAD_CAST"");		
		DEQUE_NODE* node2p = channelp->source_list;
		while(node2p)
		{
			SOURCE_T* sourcep = (SOURCE_T*)node2p->datap;
			source_node = xmlNewChild(sources_node, NULL, BAD_CAST "source", BAD_CAST"");	

			struct in_addr in;
			in.s_addr = htonl(sourcep->ip);
			char* str_ip = inet_ntoa(in);
			xmlNewProp(source_node, BAD_CAST "ip", BAD_CAST str_ip);
			char str_port[64] = {'\0'};
			sprintf(str_port, "%d", sourcep->port);
			xmlNewProp(source_node, BAD_CAST "port", BAD_CAST str_port);
			
			if(node2p->nextp == channelp->source_list)
			{
				break;
			}			
			node2p = node2p->nextp;
		}
		
		if(nodep->nextp == m_channel_list)
		{
			break;
		}
		nodep = nodep->nextp;
	}
	
	//Dumping document to stdio or file
	xmlSaveFormatFileEnc(config_file, doc, "UTF-8", 1);
	/*free the document */
	xmlFreeDoc(doc);
	xmlCleanupParser();
	//xmlMemoryDump();//debug memory for regression tests
	
	return 0;
}
#endif

int ChannelList::WriteConfig(char* config_file)
{
	FILE* filep = fopen(config_file, "w");
	if(filep == NULL)
	{
		return -1;
	}

	fprintf(filep, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
	fprintf(filep, "<channels>\n");

	DEQUE_NODE* nodep = m_channel_list;
	while(nodep)
	{
		CHANNEL_T* channelp = (CHANNEL_T*)nodep->datap;
		
		fprintf(filep, "\t<channel channel_id=\"%d\" liveid=\"%s\" bitrate=\"%d\" channel_name=\"%s\" "
			"codec_ts=\"%d\" codec_flv=\"%d\" codec_mp4=\"%d\">\n", 
			channelp->channel_id, channelp->liveid, channelp->bitrate, channelp->channel_name,
			channelp->codec_ts, channelp->codec_flv, channelp->codec_mp4);

		fprintf(filep, "\t\t<sources>\n");
		DEQUE_NODE* node2p = channelp->source_list;
		while(node2p)
		{
			SOURCE_T* sourcep = (SOURCE_T*)node2p->datap;

			struct in_addr in;
			in.s_addr = htonl(sourcep->ip);
			char* str_ip = inet_ntoa(in);
			
			fprintf(filep, "\t\t\t<source ip=\"%s\" port=\"%d\">\n",
				str_ip, sourcep->port);
			fprintf(filep, "\t\t\t</source>\n");
			
			if(node2p->nextp == channelp->source_list)
			{
				break;
			}			
			node2p = node2p->nextp;
		}
		fprintf(filep, "\t\t</sources>\n");
			
		fprintf(filep, "\t</channel>\n");		
		
		if(nodep->nextp == m_channel_list)
		{
			break;
		}
		nodep = nodep->nextp;
	}
	
	fprintf(filep, "</channels>\n");

	if(filep != NULL)
	{
		fclose(filep);
		filep = NULL;
	}
	
	return 0;
}

int ChannelList::AddChannel(CHANNEL_T * channelp)
{
	DEQUE_NODE* nodep = (DEQUE_NODE*)malloc(sizeof(DEQUE_NODE));
	if(nodep == NULL)
	{
	    fprintf(stderr, "%s: can not malloc for DEQUE_NODE\n", __FUNCTION__);
	    return -1;
	}
	memset(nodep, 0, sizeof(DEQUE_NODE));
	nodep->datap = channelp;

	m_channel_list = deque_append(m_channel_list, nodep);
			
	return 0;
}

int ChannelList::DeleteChannel(char* liveid)
{
	DEQUE_NODE* findp = NULL;
	DEQUE_NODE* nodep = m_channel_list;
	while(nodep)
	{
		CHANNEL_T* channelp = (CHANNEL_T*)nodep->datap;
		if(strcmp(channelp->liveid, liveid) == 0)
		{
			// find it
			findp = nodep;
			break;
		}

		if(nodep->nextp == m_channel_list)
		{
			break;
		}
		nodep = nodep->nextp;
	}

	if(findp != NULL)
	{
		m_channel_list = deque_remove_node(m_channel_list, findp);
		return 0;
	}
			
	return -1;
}

CHANNEL_T* ChannelList::FindChannelById(int channel_id)
{	
	DEQUE_NODE* nodep = m_channel_list;
	while(nodep)
	{
		CHANNEL_T* channelp = (CHANNEL_T*)nodep->datap;
		if(channelp->channel_id == channel_id)
		{
			// find it
			return channelp;
			break;
		}

		if(nodep->nextp == m_channel_list)
		{
			break;
		}
		nodep = nodep->nextp;
	}

	return NULL;
}


CHANNEL_T* ChannelList::FindChannelByHash(char* liveid)
{	
	DEQUE_NODE* nodep = m_channel_list;
	while(nodep)
	{
		CHANNEL_T* channelp = (CHANNEL_T*)nodep->datap;
		if(strcmp(channelp->liveid, liveid) == 0)
		{
			// find it
			return channelp;
			break;
		}

		if(nodep->nextp == m_channel_list)
		{
			break;
		}
		nodep = nodep->nextp;
	}

	return NULL;
}


