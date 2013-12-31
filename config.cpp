#include <libxml/xmlmemory.h>
#include <libxml/parser.h>

#include "public.h"
#include "config.h"

int parse_config(CONFIG_T* configp, xmlDocPtr doc, xmlNodePtr cur)
{	
	int ret = 0;

	configp->max_clip_num = MAX_CLIP_NUM + 1;
	configp->download_interval = DEFAULT_DOWNLOAD_INTERVAL;
	configp->clip_duration = DEFAULT_CLIP_DURATION;
	
	xmlNodePtr child;	
	child = cur->xmlChildrenNode;
	while((child != NULL) && (child->name != NULL))
	{
		if((!xmlStrcmp(child->name, (const xmlChar*)"ip")))
		{
			xmlChar* szValue = xmlNodeGetContent(child);
			snprintf(configp->ip, MAX_IP_LEN, "%s", (const char*)szValue);
			configp->ip[MAX_IP_LEN-1] = '\0';
			xmlFree(szValue);
		}
		else if((!xmlStrcmp(child->name, (const xmlChar*)"port")))
		{
			xmlChar* szValue = xmlNodeGetContent(child);
			configp->port = atoi((const char*)szValue);
			xmlFree(szValue);
		}
		else if((!xmlStrcmp(child->name, (const xmlChar*)"service_ip")))
		{
			xmlChar* szValue = xmlNodeGetContent(child);
			snprintf(configp->service_ip, MAX_IP_LEN, "%s", (const char*)szValue);
			configp->service_ip[MAX_IP_LEN-1] = '\0';
			xmlFree(szValue);
		}
		else if((!xmlStrcmp(child->name, (const xmlChar*)"bin_path")))
		{
			xmlChar* szValue = xmlNodeGetContent(child);
			snprintf(configp->bin_path, PATH_MAX, "%s", (const char*)szValue);
			configp->bin_path[PATH_MAX-1] = '\0';
			xmlFree(szValue);
		}
		else if((!xmlStrcmp(child->name, (const xmlChar*)"etc_path")))
		{
			xmlChar* szValue = xmlNodeGetContent(child);
			snprintf(configp->etc_path, PATH_MAX, "%s", (const char*)szValue);
			configp->etc_path[PATH_MAX-1] = '\0';
			xmlFree(szValue);
		}
		else if((!xmlStrcmp(child->name, (const xmlChar*)"log_path")))
		{
			xmlChar* szValue = xmlNodeGetContent(child);
			snprintf(configp->log_path, PATH_MAX, "%s", (const char*)szValue);
			configp->log_path[PATH_MAX-1] = '\0';
			xmlFree(szValue);
		}
		else if((!xmlStrcmp(child->name, (const xmlChar*)"html_path")))
		{
			xmlChar* szValue = xmlNodeGetContent(child);
			snprintf(configp->html_path, PATH_MAX, "%s", (const char*)szValue);
			configp->html_path[PATH_MAX-1] = '\0';
			xmlFree(szValue);
		}
		/*
		else if((!xmlStrcmp(child->name, (const xmlChar*)"work_path")))
		{
			xmlChar* szValue = xmlNodeGetContent(child);
			snprintf(configp->work_path, PATH_MAX, "%s", (const char*)szValue);
			configp->work_path[PATH_MAX-1] = '\0';
			xmlFree(szValue);
		}
		*/
		/*
		else if((!xmlStrcmp(child->name, (const xmlChar*)"channels_file")))
		{
			xmlChar* szValue = xmlNodeGetContent(child);
			snprintf(configp->channels_file, PATH_MAX, "%s", (const char*)szValue);
			configp->channels_file[PATH_MAX-1] = '\0';
			xmlFree(szValue);
		}
		*/
		else if((!xmlStrcmp(child->name, (const xmlChar*)"max_clip_num")))
		{
			xmlChar* szValue = xmlNodeGetContent(child);
			int max_clip_num = atoi((const char*)szValue);
			if(max_clip_num < MIN_CLIP_NUM)
			{
				max_clip_num = MIN_CLIP_NUM;
			}
			configp->max_clip_num = max_clip_num + 1;
			xmlFree(szValue);
		}
		else if((!xmlStrcmp(child->name, (const xmlChar*)"clip_duration")))
		{
			xmlChar* szValue = xmlNodeGetContent(child);
			int clip_duration = atoi((const char*)szValue);
			if(clip_duration < MIN_CLIP_DURATION)
			{
				clip_duration = MIN_CLIP_DURATION;
			}
			configp->clip_duration = clip_duration;
			xmlFree(szValue);
		}
		else if((!xmlStrcmp(child->name, (const xmlChar*)"download_interval")))
		{
			xmlChar* szValue = xmlNodeGetContent(child);
			int download_interval = atoi((const char*)szValue);
			if(download_interval < MIN_DOWNLOAD_INTERVAL)
			{
				download_interval = MIN_DOWNLOAD_INTERVAL;
			}
			configp->download_interval = download_interval;
			xmlFree(szValue);
		}
		else if((!xmlStrcmp(child->name, (const xmlChar*)"download_limit")))
		{
			xmlChar* szValue = xmlNodeGetContent(child);
			configp->download_limit = atol((const char*)szValue);
			xmlFree(szValue);
		}
		
		child = child->next;
	}
	
	return ret;
}


int config_read(CONFIG_T* configp, char* file_name)
{
	int ret = 0;
	
	xmlDocPtr doc;
	xmlNodePtr cur;
	
	doc=xmlParseFile(file_name);
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

	if (xmlStrcmp(cur->name, (const xmlChar *)"config") != 0)
	{	
		fprintf(stderr, "%s: root node is not 'config'!\n", __FUNCTION__);
		xmlFreeDoc(doc);
		return -1;
	}

	ret = parse_config(configp, doc, cur);	
	xmlFreeDoc(doc);
	
	return ret;
}

int config_write(CONFIG_T* configp, char* file_name)
{
	FILE* filep = fopen(file_name, "w");
	if(filep == NULL)
	{
		return -1;
	}

	fprintf(filep, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
	fprintf(filep, "<config>\n");
	
	fprintf(filep, "\t<ip>%s</ip>\n", configp->ip);
	fprintf(filep, "\t<port>%u</port>\n", configp->port);
	fprintf(filep, "\t<service_ip>%s</service_ip>\n", configp->service_ip);
	fprintf(filep, "\t<bin_path>%s</bin_path>\n", configp->bin_path);
	fprintf(filep, "\t<etc_path>%s</etc_path>\n", configp->etc_path);
	fprintf(filep, "\t<log_path>%s</log_path>\n", configp->log_path);
	fprintf(filep, "\t<html_path>%s</html_path>\n", configp->html_path);
	fprintf(filep, "\t<max_clip_num>%d</max_clip_num>\n", configp->max_clip_num - 1);
	fprintf(filep, "\t<clip_duration>%d s</clip_duration>\n", configp->clip_duration);
	fprintf(filep, "\t<download_interval>%d ms</download_interval>\n", configp->download_interval);
	fprintf(filep, "\t<download_limit>%ld bps</download_limit>\n", configp->download_limit);
	
	fprintf(filep, "</config>\n");

	if(filep != NULL)
	{
		fclose(filep);
		filep = NULL;
	}
	
	return 0;
}



