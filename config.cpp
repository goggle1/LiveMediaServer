#include <libxml/xmlmemory.h>
#include <libxml/parser.h>

#include "config.h"

int parse_config(CONFIG_T* configp, xmlDocPtr doc, xmlNodePtr cur)
{	
	int ret = 0;
	
	xmlNodePtr child;	
	child = cur->xmlChildrenNode;
	while((child != NULL) && (child->name != NULL))
	{
		if((!xmlStrcmp(child->name, (const xmlChar*)"ip")))
		{
			xmlChar* szValue = xmlNodeGetContent(child);
			snprintf(configp->ip, MAX_IP_LEN-1, "%s", (const char*)szValue);
			configp->ip[MAX_IP_LEN-1] = '\0';
			xmlFree(szValue);
		}
		else if((!xmlStrcmp(child->name, (const xmlChar*)"port")))
		{
			xmlChar* szValue = xmlNodeGetContent(child);
			configp->port = atoi((const char*)szValue);
			xmlFree(szValue);
		}
		else if((!xmlStrcmp(child->name, (const xmlChar*)"work_path")))
		{
			xmlChar* szValue = xmlNodeGetContent(child);
			snprintf(configp->work_path, PATH_MAX-1, "%s", (const char*)szValue);
			configp->work_path[PATH_MAX-1] = '\0';
			xmlFree(szValue);
		}
		else if((!xmlStrcmp(child->name, (const xmlChar*)"channels_file")))
		{
			xmlChar* szValue = xmlNodeGetContent(child);
			snprintf(configp->channels_file, PATH_MAX-1, "%s", (const char*)szValue);
			configp->channels_file[PATH_MAX-1] = '\0';
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

