
#include <stdlib.h>
#include <string.h>
#include <stdlib.h>

#include "dequeH.h"

int	 	dequeh_init(DEQUEH_T* dequep)
{
	dequep->headp = NULL;
	dequep->count = 0;

	return 0;
}

int 		dequeh_append(DEQUEH_T* dequep, void* elementp)
{
	if(dequep == NULL)
	{
		return -1;
	}
	
	DEQUEH_NODE* nodep = (DEQUEH_NODE*)malloc(sizeof(DEQUEH_NODE));
	if(nodep == NULL)
	{
		return -1;
	}
	nodep->nextp = NULL;
	nodep->prevp = NULL;
	nodep->elementp = elementp;

	if(dequep->headp == NULL)
	{
		nodep->nextp = nodep;
		nodep->prevp = nodep;
		dequep->headp = nodep;
		dequep->count ++;
		return 0;
	}

	DEQUEH_NODE* tailp = dequep->headp->prevp;
	tailp->nextp = nodep;
	nodep->prevp = tailp;
	nodep->nextp = dequep->headp;
	dequep->headp->prevp = nodep;
	dequep->count ++;
	return 0;
}

void 		dequeh_release(DEQUEH_T* dequep, RELEASE_FUNCTION release_it)
{
	if(dequep == NULL)
	{
		return;
	}

	DEQUEH_NODE* nodep = dequep->headp;
	while(nodep != NULL)
	{
		DEQUEH_NODE* tempp = nodep->nextp;
	
		release_it(nodep->elementp);		
		free(nodep);

		if(tempp == dequep->headp)
		{
			break;
		}
		nodep = tempp;
	}
	
	dequep->headp = NULL;
	dequep->count = 0;
}


void* 	dequeh_remove_head(DEQUEH_T* dequep)
{
	if(dequep == NULL)
	{
		return NULL;
	}

	if(dequep->headp == NULL)
	{
		return NULL;
	}
	
	if(dequep->count == 1)
	{
		DEQUEH_NODE* tempp = dequep->headp;
		dequep->count = 0;
		dequep->headp = NULL;
		
		void* elementp = tempp->elementp;
		free(tempp);
		
		return elementp;
	}
	
	DEQUEH_NODE* headp = dequep->headp;
	DEQUEH_NODE* tailp = headp->prevp;
	DEQUEH_NODE* tempp = headp->nextp;
	tempp->prevp = headp->prevp;
	tailp->nextp = tempp;
	
	dequep->count --;
	dequep->headp = tempp;

	void* elementp = headp->elementp;
	free(headp);

	return elementp;		
}


