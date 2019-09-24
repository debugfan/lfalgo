#ifndef MEM_UTILS_H
#define MEM_UTILS_H

#include "lfdef.h"

#ifdef __cplusplus
extern "C"
{
#endif

void *memory_alloc(size_t size, unsigned long tag);
void memory_free(void *ptr);

#ifndef _NTDDK_
#include <stdio.h>
static int print_leak_info(int i, int j, int alloc_num, int free_num, void *handle_data)
{
	static int line = 0;
	if (line != i)
	{
		line = i;
		printf("\n");
	}
	return printf(" \t%c/x%02x(%d, %d)",
		(j >= 20 && j < 127) ? j : '?',
		j,
		alloc_num,
		free_num);
}
#endif

typedef int(*leaked_handle_t) (int i, int j, int alloc_num, int free_num, void *handle_data);
BOOL check_memory(leaked_handle_t handle, void *handle_data);

#ifdef __cplusplus
}
#endif

#endif
