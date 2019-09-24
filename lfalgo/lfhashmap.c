#include "lfhashmap.h"
#include <string.h>
#include <stddef.h>

#define HASHMAP_BUCKET_NUMBER(map)   (sizeof(map->buckets)/sizeof(map->buckets[0]))

#define HASHMAP_DEFAULT_HASH(key)   ((((uintptr_t)key) >> 3) | (((uintptr_t)key) << ((sizeof(void *)*8) - 3)))

int lfhashmap_get_bucket_idx(lfhashmap_t *map, const void *key)
{
    if(map->hash == NULL)
    {
        return HASHMAP_DEFAULT_HASH(key) % HASHMAP_BUCKET_NUMBER(map);
    }
    else
    {
        return map->hash(key) % HASHMAP_BUCKET_NUMBER(map);
    }
}

void lfhashmap_init(lfhashmap_t *map, lfhashmap_hash_func_t hash, lflistmap_env_t *env, unsigned long tag)
{
    int i;
    if(env != NULL)
    {
        memcpy(&map->env, env, sizeof(lflistmap_env_t));
    }
    else
    {
        memset(&map->env, 0, sizeof(lflistmap_env_t));
    }
    for(i = 0; i < HASHMAP_BUCKET_NUMBER(map); i++)
    {
        lflistmap_init(&map->buckets[i], env, tag);
    }
    map->hash = hash;
    map->tag = tag;
}

BOOL lfhashmap_add(lfhashmap_t *map, const void *key, void *value, lflistmap_entry_t **added_entry)
{
    int idx;
    idx = lfhashmap_get_bucket_idx(map, key);
    return lflistmap_add(&map->buckets[idx], key, value, added_entry);
}

void lfhashmap_remove(lfhashmap_t *map, void *key)
{
    int idx;
    idx = lfhashmap_get_bucket_idx(map, key);
    lflistmap_remove(&map->buckets[idx], key);   
}

lflistmap_entry_t *lfhashmap_get_entry(lfhashmap_t *map, const void *key)
{
    int idx;
    idx = lfhashmap_get_bucket_idx(map, key);
    return lflistmap_get_entry(&map->buckets[idx], key);     
}

void lfhashmap_remove_entry(lfhashmap_t *map, lflistmap_entry_t *entry)
{
    int idx;
    idx = lfhashmap_get_bucket_idx(map, lflistmap_get_key(&map->env, entry));
    lflistmap_remove_entry(&map->buckets[idx], entry);
}

void lfhashmap_release_entry(lfhashmap_t *map, lflistmap_entry_t *entry)
{
    int idx;
    idx = lfhashmap_get_bucket_idx(map, lflistmap_get_key(&map->env, entry));
    lflistmap_release_entry(&map->buckets[idx], entry);
}

void lfhashmap_traverse(lfhashmap_t *map, lfhashmap_traverse_callback_t handle, void *in_data, void *out_data)
{
    int i;
    for(i = 0; i < HASHMAP_BUCKET_NUMBER(map); i++)
    {
        lflistmap_traverse(&map->buckets[i], handle, in_data, out_data);
    }
}

void lfhashmap_clear(lfhashmap_t *map, int close_flag)
{
    int i;
    for(i = 0; i < HASHMAP_BUCKET_NUMBER(map); i++)
    {
        lflistmap_clear(&map->buckets[i], close_flag);
    }
}
