#include "pattern_list.h"
#include "wildmat.h"
#include "mem_utils.h"

#define PATTERN_LIST_TAG 'RTTP'

void init_pattern_list(lflist_t *list)
{
	memset(list, 0, sizeof(lflist_t));
}

BOOL add_pattern_to_list(lflist_t *list, const char *pattern, int length, void *data)
{
	int total;
	pattern_node_t *node;

	if (length == -1)
	{
		length = (int)strlen(pattern);
	}
	total = sizeof(pattern_node_t) + length + 1;
	node = memory_alloc(total, PATTERN_LIST_TAG);
	if (node != NULL)
	{
		memset(node, 0, sizeof(pattern_node_t));
		node->data = data;
		memcpy(node->pattern, pattern, length);
		*(node->pattern + length) = '\0';
		lflist_add_entry(list, &node->entry);
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

int remove_pattern_compare(lflist_entry_t *entry, const void *condi)
{
	pattern_node_t *node = (pattern_node_t *)entry;
	if (node != NULL)
	{
		if (0 == strcmp(node->pattern, condi))
		{
			return 0;
		}
	}
	return 1;
}

void remove_pattern_from_list(lflist_t *list, const char *pattern)
{
	lflist_remove(list, remove_pattern_compare, pattern, memory_free);
}

BOOL match_pattern_list_callback(lflist_entry_t *entry, const void *in_data, void *out_data)
{
	pattern_node_t *node = (pattern_node_t *)entry;
	if (node != NULL)
	{
		if (0 != wildmat(in_data, node->pattern))
		{
			*(void **)out_data = node->data;
			return FALSE;
		}
	}
	return TRUE;
}

void *match_pattern_list(lflist_t *list, const char *text)
{
	void *data = NULL;
	lflist_traverse(list, match_pattern_list_callback, (char *)text, &data);
	return data;
}

typedef struct {
	char *buf;
	size_t len;
	size_t off;
} print_pattern_list_callback_data_t;

BOOL print_pattern_list_callback(lflist_entry_t *entry, const void *in_data, void *out_data)
{
	pattern_node_t *node = (pattern_node_t *)entry;
	int delim = (int)(intptr_t)in_data;
	print_pattern_list_callback_data_t *cbd = out_data;
	size_t tlen;
	if (node != NULL && cbd != NULL)
	{
		tlen = strlen((char *)node->pattern);
		if (1 + cbd->off + tlen > cbd->len)
		{
			return FALSE;
		}
		if (cbd->buf != NULL)
		{
			cbd->buf[cbd->off] = (char)delim;
			memcpy(cbd->buf + cbd->off + 1, node->pattern, tlen);
		}
		cbd->off = cbd->off + 1 + tlen;
	}
	return TRUE;
}

size_t print_pattern_list(lflist_t *list, char *buf, size_t len, int delim)
{
	print_pattern_list_callback_data_t cbd;
	cbd.buf = buf;
	cbd.len = len;
	cbd.off = 0;
	lflist_traverse(list, print_pattern_list_callback, (void *)delim, &cbd);
	return cbd.off;
}

void clear_pattern_list(lflist_t *list, BOOL destroy_flag)
{
	lflist_clear(list, memory_free, destroy_flag);
}
