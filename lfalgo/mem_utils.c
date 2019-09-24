#include "mem_utils.h"

#ifdef _DEBUG
#define USE_MEMORY_COUNTER
#endif

#ifdef USE_MEMORY_COUNTER
#define ALLOC_HEADER_SIZE sizeof(void *)*4
#else
#define ALLOC_HEADER_SIZE 0
#endif

typedef struct {
	long volatile alloc_num;
	long volatile free_num;
} memory_counter_t;

memory_counter_t memory_counter[4][256] = { 0 };

void *internal_memory_alloc(size_t size, unsigned int tag)
{
#ifdef _NTDDK_
	return ExAllocatePoolWithTag(NonPagedPool, size, tag);
#else
	return malloc(size);
#endif
}

void internal_memory_free(void *p)
{
#ifdef _NTDDK_
	ExFreePool(p);
#else
	free(p);
#endif
}

void memory_count(unsigned long tag, int alloc_or_free)
{
	int i, j;
	for (i = 0; i < 4; i++)
	{
		j = ((tag >> (i * 8)) & 0xFF);
		if (alloc_or_free != 0)
		{
			InterlockedIncrement(&memory_counter[i][j].alloc_num);
		}
		else
		{
			InterlockedIncrement(&memory_counter[i][j].free_num);
		}
	}
}

void *memory_alloc(size_t size, unsigned long tag)
{
	void *ptr;
	size_t total_size;
	total_size = size + ALLOC_HEADER_SIZE;
	ptr = internal_memory_alloc(total_size, tag);
	if (ptr != NULL)
	{
#ifdef USE_MEMORY_COUNTER
        *(unsigned long *)ptr = tag;
		memory_count(tag, 1);
#endif
		return (char *)ptr + ALLOC_HEADER_SIZE;
	}
	else
	{
		return NULL;
	}
}

void memory_free(void *ptr)
{
	void *actual = (char *)ptr - ALLOC_HEADER_SIZE;
	if (actual != NULL)
	{
#ifdef USE_MEMORY_COUNTER
		memory_count(*(unsigned long *)actual, 0);
#endif
	}
	internal_memory_free(actual);
}

BOOL check_memory(leaked_handle_t handle, void *handle_data)
{
	int i, j;
	int leaked = 0;
	for (i = 0; i < 4; i++)
	{
		for (j = 0; j < 256; j++)
		{
			if (memory_counter[i][j].alloc_num != memory_counter[i][j].free_num)
			{
				if (handle != NULL)
				{
					handle(i, j, memory_counter[i][j].alloc_num, memory_counter[i][j].free_num, handle_data);
				}
				if (leaked == 0)
				{
					leaked = 1;
				}
			}
		}
	}

	return (leaked == 0 ? TRUE : FALSE);
}
