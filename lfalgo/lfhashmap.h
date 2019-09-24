#ifndef LFHASHMAP_H
#define LFHASHMAP_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "lflistmap.h"

typedef lflistmap_entry_t lfhashmap_entry_t;

typedef unsigned int (*lfhashmap_hash_func_t)(const void *key);

typedef struct {
    lflistmap_t buckets[0x10000];
    lflistmap_env_t env;
    lfhashmap_hash_func_t hash;
    unsigned long tag;
} lfhashmap_t;

void lfhashmap_init(lfhashmap_t *map, lfhashmap_hash_func_t hash, lflistmap_env_t *env, unsigned long tag);
BOOL lfhashmap_add(lfhashmap_t *lmap, const void *key, void *value, lflistmap_entry_t **added_entry);
void lfhashmap_remove(lfhashmap_t *map, void *key);
lflistmap_entry_t *lfhashmap_get_entry(lfhashmap_t *map, const void *key);
void lfhashmap_remove_entry(lfhashmap_t *map, lflistmap_entry_t *entry);
void lfhashmap_release_entry(lfhashmap_t *map, lflistmap_entry_t *entry);
void lfhashmap_clear(lfhashmap_t *map, int close_flag);

typedef BOOL(*lfhashmap_traverse_callback_t)(lfhashmap_entry_t *entry, void *in_data, void *out_data);
void lfhashmap_traverse(lfhashmap_t *map, lfhashmap_traverse_callback_t handle, void *in_data, void *out_data);

#ifdef __cplusplus
}
#endif

#endif
