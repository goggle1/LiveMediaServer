
#ifndef __DEQUEH_H__
#define __DEQUEH_H__
// deque with head

typedef struct dequeh_node
{
	struct dequeh_node* nextp;
	struct dequeh_node* prevp;
	void* 	elementp;
} DEQUEH_NODE;

typedef struct dequeh_t
{
	int 			count;
	DEQUEH_NODE*	headp;
} DEQUEH_T;

typedef void (*RELEASE_FUNCTION) (void* elementp);

int	 		dequeh_init(DEQUEH_T* dequep);
int 		dequeh_append(DEQUEH_T* dequep, void* elementp);
void 		dequeh_release(DEQUEH_T* dequep, RELEASE_FUNCTION release_it);
void*	 	dequeh_remove_head(DEQUEH_T* dequep);

#endif

