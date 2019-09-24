#ifndef LFLIST_H
#define LFLIST_H

#include "lfdef.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct _lflist_link_t {
    struct _lflist_link_t * volatile next;
    struct _lflist_link_t * volatile back_off;
    struct _lflist_link_t * volatile free_next;
} lflist_link_t;

typedef struct _lflist_entry_t {
    lflist_link_t link;
    long volatile ref_cnt;
} lflist_entry_t;

typedef lflist_entry_t *(*lflist_alloc_entry_func_t)(void *key, void *value, unsigned long tag);
typedef void (*lflist_free_entry_func_t)(lflist_entry_t *entry);

typedef int (*lflist_entry_cmp_func_t)(lflist_entry_t *entry, const void *condi);
typedef BOOL(*lflist_update_func_t)(lflist_entry_t *entry, void *data, BOOL *require_retry);
typedef BOOL (*lflist_smear_func_t)(lflist_entry_t *entry, BOOL force_flag);

typedef struct _lflist_t {
    lflist_link_t list;
    long volatile count;
    long volatile ref_cnt;
    long volatile close_flag;
} lflist_t;

void lflist_init(lflist_t *lst);

BOOL lflist_add_entry(lflist_t *lst, lflist_entry_t *entry);
BOOL lflist_insert_entry(lflist_t *lst, lflist_entry_t *entry);
int lflist_remove_entry(lflist_t *lst, lflist_entry_t *entry, lflist_smear_func_t smear_func);

int lflist_remove(lflist_t *lst, lflist_entry_cmp_func_t func, const void *condi, lflist_free_entry_func_t entry_free);

lflist_entry_t *lflist_get_entry(lflist_t *lst, lflist_entry_cmp_func_t func, const void *condi);
void lflist_release_entry(lflist_t *lst, lflist_entry_t *entry, lflist_free_entry_func_t entry_free);
void lflist_clear(lflist_t *lst, lflist_free_entry_func_t entry_free, int close_flag);

lflist_entry_t *lflist_pop_entry(lflist_t *lst, lflist_smear_func_t mark_func);

typedef BOOL (*lflist_traverse_callback_t)(lflist_entry_t *entry, void *in_data, void *out_data);
BOOL lflist_traverse(lflist_t *lst, lflist_traverse_callback_t handle, void *in_data, void *out_data);
BOOL lflist_exists(lflist_t *lst, lflist_entry_cmp_func_t cmp_func, const void *condi);

#ifdef __cplusplus
}
#endif

#endif
