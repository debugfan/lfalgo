#ifndef LFLISTMAP_H
#define LFLISTMAP_H

#include "lfdef.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct _lflistmap_link_t {
    struct _lflistmap_link_t * volatile next;
    struct _lflistmap_link_t * volatile back_off;
    struct _lflistmap_link_t * volatile free_next;
} lflistmap_link_t;

typedef struct _lflistmap_entry_t {
    lflistmap_link_t link;
    long volatile ref_cnt;
} lflistmap_entry_t;

typedef lflistmap_entry_t *(*lflistmap_alloc_entry_func_t)(void *key, void *value, unsigned long tag);
typedef void (*lflistmap_free_entry_func_t)(lflistmap_entry_t *entry);

typedef void *(*lflistmap_get_key_func_t)(lflistmap_entry_t *entry);
typedef int (*lflistmap_key_cmp_func_t)(const void *key1, const void *key2);
typedef BOOL (*lflistmap_update_value_func_t)(lflistmap_entry_t *entry, void *value, BOOL *require_retry);
typedef BOOL (*lflistmap_mark_value_func_t)(lflistmap_entry_t *entry, BOOL force_flag);

typedef struct _lflistmap_env_t {
    lflistmap_alloc_entry_func_t alloc_entry;
    lflistmap_free_entry_func_t free_entry;
    lflistmap_get_key_func_t get_key;
    lflistmap_key_cmp_func_t key_cmp;
    lflistmap_update_value_func_t update_value;
    lflistmap_mark_value_func_t mark_value;
} lflistmap_env_t;

typedef struct _lflistmap_t {
    lflistmap_link_t list;
    lflistmap_env_t env;
    unsigned long tag;
    long volatile count;
    long volatile ref_cnt;
    long volatile close_flag;
} lflistmap_t;

static __inline void *lflistmap_get_key(lflistmap_env_t *env, lflistmap_entry_t *entry)
{
    if(env != NULL && env->get_key != NULL)
    {
        return env->get_key(entry);
    }
    else
    {
        return NULL;
    }
}

void lflistmap_init(lflistmap_t *lmap, lflistmap_env_t *env, unsigned long tag);

BOOL lflistmap_add(lflistmap_t *lmap, void *key, void *value, lflistmap_entry_t **added_entry);
void lflistmap_remove(lflistmap_t *lmap, const void *key);
BOOL lflistmap_exists(lflistmap_t *lmap, const void *key);

lflistmap_entry_t *lflistmap_get_entry(lflistmap_t *lmap, const void *key);
void lflistmap_remove_entry(lflistmap_t *lmap, lflistmap_entry_t *entry);
void lflistmap_release_entry(lflistmap_t *lmap, lflistmap_entry_t *entry);
void lflistmap_clear(lflistmap_t *lmap, int close_flag);

lflistmap_entry_t *lflistmap_pop_entry(lflistmap_t *lmap);
void lflistmap_try_free_entry(lflistmap_t *lmap, lflistmap_entry_t *entry);

typedef BOOL (*lflistmap_traverse_callback_t)(lflistmap_entry_t *entry, void *in_data, void *out_data);
BOOL lflistmap_traverse(lflistmap_t *lmap, lflistmap_traverse_callback_t handle, void *in_data, void *out_data);

#define lflistset_t                lflistmap_t
#define lflistset_init             lflistmap_init
#define lflistset_add(l, k)        lflistmap_add(l, k, NULL, NULL)
#define lflistset_remove           lflistmap_remove
#define lflistset_clear            lflistmap_clear

#ifdef __cplusplus
}
#endif

#endif
