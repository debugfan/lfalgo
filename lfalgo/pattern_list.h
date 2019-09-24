#ifndef PATTERN_LIST_H
#define PATTERN_LIST_H

#include "lflist.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct {
	lflist_entry_t entry;
	void *data;
	char pattern[1];
} pattern_node_t;

void init_pattern_list(lflist_t *list);
BOOL add_pattern_to_list(lflist_t *list, const char *pattern, int length, void *data);
void remove_pattern_from_list(lflist_t *list, const char *pattern);
size_t print_pattern_list(lflist_t *list, char *buf, size_t len, int delim);
void *match_pattern_list(lflist_t *list, const char *text);
void clear_pattern_list(lflist_t *list, BOOL destroy_flag);

#ifdef __cplusplus
}
#endif

#endif
