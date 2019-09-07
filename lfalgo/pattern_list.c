#include "pattern_list.h"
#include "wildmat.h"

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
		length = strlen(pattern);
	}
	total = sizeof(pattern_node_t) + length + 1;
	node = malloc(total);
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
	lflist_remove(list, remove_pattern_compare, pattern, free);
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
	lflist_traverse(list, match_pattern_list_callback, text, &data);
	return data;
}

void clear_pattern_list(lflist_t *list, BOOL destroy_flag)
{
	lflist_clear(list, free, destroy_flag);
}
