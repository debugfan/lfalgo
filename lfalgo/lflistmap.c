#include "lflistmap.h"
#include <stdlib.h>
#include <string.h>

#define LFLISTMAP_LINK_TO_ENTRY(link) ((lflistmap_entry_t *) link)

lflistmap_entry_t *lflistmap_alloc_entry(lflistmap_env_t *env, void *key, void *value, unsigned long tag)
{
	if (env != NULL && env->alloc_entry != NULL)
	{
		return env->alloc_entry(key, value, tag);
	}
	else
	{
		return NULL;
	}
}

void lflistmap_free_entry(lflistmap_env_t *env, lflistmap_entry_t *entry)
{
    if(env != NULL && env->free_entry != NULL)
    {
        env->free_entry(entry);
    }
}

__inline int lflistmap_key_cmp(lflistmap_env_t *env, const void *key1, const void *key2)
{
    if(env != NULL && env->key_cmp != NULL)
    {
        return env->key_cmp(key1, key2);
    }
    else
    {
        return (key1 < key2 ? -1 : (key1 == key2 ? 0 : 1));
    }
}

__inline BOOL lflistmap_update_value(lflistmap_env_t *env, lflistmap_entry_t *entry, void *value, BOOL *retry)
{
    if(env != NULL && env->update_value != NULL)
    {
        return env->update_value(entry, value, retry);
    }
    else
    {
        return TRUE;
    }
}

void lflistmap_init(lflistmap_t *lmap, lflistmap_env_t *env, unsigned long tag)
{
    memset(lmap, 0, sizeof(lflistmap_t));
    if(env != NULL)
    {
        memcpy(&lmap->env, env, sizeof(lflistmap_env_t));
    }
    else
    {
        memset(&lmap->env, 0, sizeof(lflistmap_env_t));
    }
    lmap->tag = tag;
}

__inline lflistmap_link_t * lflistmap_get_unmarked(lflistmap_link_t *p)
{
    if(((uintptr_t)p & 1) != 0)
    {
        return (lflistmap_link_t *)((uintptr_t)p ^ 1);
    }
    else
    {
        return p;
    }
}

void lflistmap_push_free_list(lflistmap_t *lmap, lflistmap_entry_t *entry)
{
    lflistmap_link_t *head;
    lflistmap_link_t *last;
    
    if(entry != NULL)
    {
        for(last = &entry->link; last->free_next != NULL; last = last->free_next)
        {
            revolve(1);
        }
        
        for (;;) {
            head = lmap->list.free_next;
            last->free_next = head;
            if (head == InterlockedCompareExchangePointer(&lmap->list.free_next,
                entry,
                head))
            {
                break;
            }
        }
    }
}

void lflistmap_retrace(lflistmap_link_t * volatile *pprev, lflistmap_link_t *volatile *pcur)
{
    lflistmap_link_t * volatile prev_next;
    prev_next = (*pprev)->next;
    (*pcur) = lflistmap_get_unmarked(prev_next);
    while(((uintptr_t)prev_next & 1) != 0)
    {
        (*pprev) = (*pcur)->back_off;
        prev_next = (*pprev)->next;
        (*pcur) = lflistmap_get_unmarked(prev_next);
    }
}

void lflistmap_try_free_entry(lflistmap_t *lmap, lflistmap_entry_t *entry)
{
    if(lmap->ref_cnt == 0 && entry->ref_cnt == 0)
    {
        lflistmap_free_entry(&lmap->env, entry);
    }
    else
    {
        lflistmap_push_free_list(lmap, entry);
    }    
}

static __inline BOOL lflistmap_remove_marked_entry(lflistmap_t *lmap, lflistmap_link_t *cur, lflistmap_link_t *prev, lflistmap_link_t *cur_next)
{
    if(cur == InterlockedCompareExchangePointer(&prev->next, lflistmap_get_unmarked(cur_next), cur))
    {
        InterlockedDecrement(&lmap->count);
        InterlockedDecrement(&LFLISTMAP_LINK_TO_ENTRY(cur)->ref_cnt);
        lflistmap_push_free_list(lmap, LFLISTMAP_LINK_TO_ENTRY(cur));
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

BOOL lflistmap_insert_entry_internal(lflistmap_t *lmap, lflistmap_entry_t *neo, lflistmap_link_t *prev, lflistmap_link_t *next)
{
    neo->link.next = next;
    if (next == InterlockedCompareExchangePointer(&prev->next,
        &neo->link.next,
        next))
    {
        InterlockedIncrement(&lmap->count);
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

BOOL lflistmap_add_internal(lflistmap_t *lmap, void *key, void *value, lflistmap_entry_t **added_entry)
{
    lflistmap_entry_t * neo;
    lflistmap_link_t * volatile prev;
    lflistmap_link_t * volatile cur;
    lflistmap_link_t * cur_next;
    BOOL r = FALSE;
    
    neo = lflistmap_alloc_entry(&lmap->env, key, value, lmap->tag);
    if(neo != NULL)
    {
        if(added_entry != NULL)
        {
            InterlockedIncrement(&neo->ref_cnt);
        }
        prev = &lmap->list;
        cur = prev->next;
        for(;;)
        {
            if(cur == NULL)
            {
                if(lflistmap_insert_entry_internal(lmap, neo, prev, cur))
                {
                    if(added_entry != NULL)
                    {
                        *added_entry = neo;
                    }
                    r = TRUE;
                    break;
                }
                else
                {
                    lflistmap_retrace(&prev, &cur);
                    continue;
                }
            }
            
            cur_next = cur->next;
            
            if(((uintptr_t)cur_next & 1) == 0)
            {
                int r_cmp;
                r_cmp = lflistmap_key_cmp(&lmap->env, lflistmap_get_key(&lmap->env, LFLISTMAP_LINK_TO_ENTRY(cur)), key);
                if(r_cmp < 0)
                {
                    prev = cur;
                    cur = cur_next;
                    continue;
                }
                else if(r_cmp == 0)
                {
                    lflistmap_entry_t *cur_entry;
                    BOOL retry = FALSE;
                    cur_entry = LFLISTMAP_LINK_TO_ENTRY(cur);
                    InterlockedIncrement(&cur_entry->ref_cnt);
                    if(lflistmap_update_value(&lmap->env, cur_entry, value, &retry))
                    {
                        if(added_entry != NULL)
                        {
                            *added_entry = cur_entry;
                        }
                        else
                        {
                            InterlockedDecrement(&cur_entry->ref_cnt);
                        }
                        lflistmap_free_entry(&lmap->env, neo);
                        r = TRUE;
                        break;
                    }
                    else
                    {
                        InterlockedDecrement(&cur_entry->ref_cnt);
                        if(retry != FALSE)
                        {
                            continue;
                        }
                        else
                        {
                            if(added_entry != NULL)
                            {
                                *added_entry = NULL;
                            }
                            else
                            {
                                InterlockedDecrement(&cur_entry->ref_cnt);
                            }
                            lflistmap_free_entry(&lmap->env, neo);
                            r = FALSE;
                            break;
                        }
                    }
                }
                else
                {
                    if(lflistmap_insert_entry_internal(lmap, neo, prev, cur))
                    {
                        if(added_entry != NULL)
                        {
                            *added_entry = neo;
                        }
                        r = TRUE;
                        break;
                    }
                    else
                    {
                        lflistmap_retrace(&prev, &cur);
                        continue;
                    }
                }
            }
            
            if(((uintptr_t)cur_next & 1) != 0)
            {
                if(lflistmap_remove_marked_entry(lmap, cur, prev, cur_next))
                {
                    cur = lflistmap_get_unmarked(cur_next);
                }
                else
                {
                    lflistmap_retrace(&prev, &cur);
                }
            }
        }
    }
    return r;
}

BOOL lflistmap_add(lflistmap_t *lmap, void *key, void *value, lflistmap_entry_t **added_entry)
{
    BOOL r = FALSE;
  
    if(lmap->close_flag == 0)
    {
        InterlockedIncrement(&lmap->ref_cnt);
        if(lmap->close_flag == 0)
        {
            r = lflistmap_add_internal(lmap, key, value, added_entry);
        }
        InterlockedDecrement(&lmap->ref_cnt);
    }
    
    return r;
}

void lflistmap_collect_garbage(lflistmap_t *lmap, BOOL force_collect)
{
    lflistmap_link_t *head;
    lflistmap_link_t *link;
    lflistmap_link_t *volatile *prev_next_p;
    link = InterlockedExchangePointer(&lmap->list.free_next, NULL);
    if(link != NULL)
    {
        if(lmap->ref_cnt != 0)
        {
            if(force_collect)
            {
                while(lmap->ref_cnt > 0)
                {
                    revolve(1);
                }
            }
            else
            {                
                lflistmap_push_free_list(lmap, LFLISTMAP_LINK_TO_ENTRY(link));
                return;
            }
        }
        
        head = link;
        prev_next_p = &head;
        while(link != NULL)
        {
            lflistmap_link_t *next;
            next = link->free_next;
            
            if(force_collect)
            {
                while(LFLISTMAP_LINK_TO_ENTRY(link)->ref_cnt > 0)
                {
                    revolve(1);
                }
            }
            
            if(LFLISTMAP_LINK_TO_ENTRY(link)->ref_cnt > 0)
            {
                prev_next_p = &link->free_next;
                link = next;
            }
            else
            {
                *prev_next_p = next;
                lflistmap_free_entry(&lmap->env, LFLISTMAP_LINK_TO_ENTRY(link));
                link = next;
            }
        }
        lflistmap_push_free_list(lmap, LFLISTMAP_LINK_TO_ENTRY(head));
    }
}

void lflistmap_release_entry(lflistmap_t *lmap, lflistmap_entry_t *entry)
{
    if(0 == InterlockedDecrement(&entry->ref_cnt))
    {
        lflistmap_collect_garbage(lmap, FALSE);
    }
}

lflistmap_link_t *lflistmap_mark_link(lflistmap_link_t *link)
{
    lflistmap_link_t *next;
    for(;;)
    {
        next = link->next;
        if(((uintptr_t)next & 1) != 0)
        {
            return next;
        }
        else
        {
            if(next == InterlockedCompareExchangePointer(&link->next, (void *)((uintptr_t)next | 1), next))
            {
                return (void *)((uintptr_t)next | 1);
            }
            else
            {
                continue;
            }
        }
    }
}

BOOL lflistmap_traverse(lflistmap_t *lmap, lflistmap_traverse_callback_t handle, void *in_data, void *out_data)
{
    lflistmap_link_t * volatile prev;
    lflistmap_link_t * volatile cur;
    lflistmap_link_t *cur_next = NULL;
	BOOL ret = TRUE;

    InterlockedIncrement(&lmap->ref_cnt);
    prev = &lmap->list;
    cur = prev->next;
    for(;cur != NULL;)
    {
        cur_next = cur->next;
        
        if(((uintptr_t)cur_next & 1) == 0)
        {
            if(!handle(LFLISTMAP_LINK_TO_ENTRY(cur), in_data, out_data))
            {
				ret = FALSE;
                break;
            }
            else
            {
                prev = cur;
                cur = cur_next;
                continue;
            }
        }
        
        if(((uintptr_t)cur_next & 1) != 0)
        {
            if(lflistmap_remove_marked_entry(lmap, cur, prev, cur_next))
            {
                cur = lflistmap_get_unmarked(cur_next);
            }
            else
            {
                lflistmap_retrace(&prev, &cur);
            }
        }
    }
    InterlockedDecrement(&lmap->ref_cnt);
	return ret;
}

lflistmap_entry_t *lflistmap_get_entry(lflistmap_t *lmap, const void *key)
{
    lflistmap_link_t * volatile prev;
    lflistmap_link_t * volatile cur;
    lflistmap_link_t *cur_next = NULL;

    InterlockedIncrement(&lmap->ref_cnt);
    prev = &lmap->list;
    cur = prev->next;
    for(;cur != NULL;)
    {
        cur_next = cur->next;
        
        if(((uintptr_t)cur_next & 1) == 0)
        {
            lflistmap_entry_t *cur_entry;
            int r_cmp;
            
            cur_entry = LFLISTMAP_LINK_TO_ENTRY(cur);
            r_cmp = lflistmap_key_cmp(&lmap->env, lflistmap_get_key(&lmap->env, cur_entry), key);
            
            if(r_cmp == 0)
            {
                InterlockedIncrement(&cur_entry->ref_cnt);
                cur_next = cur_entry->link.next;
                if(((uintptr_t)cur_next & 1) == 0)
                {
                    break;
                }
                else
                {
                    InterlockedDecrement(&cur_entry->ref_cnt);
                    continue;
                }
            }
            else if(r_cmp < 0)
            {
                prev = cur;
                cur = lflistmap_get_unmarked(cur_next);
                continue;
            }
            else
            {
                cur = NULL;
                break;
            }
        }
        
        if(((uintptr_t)cur_next & 1) != 0)
        {
            if(lflistmap_remove_marked_entry(lmap, cur, prev, cur_next))
            {
                cur = lflistmap_get_unmarked(cur_next);
            }
            else
            {
                lflistmap_retrace(&prev, &cur);
            }
        }
    }
    InterlockedDecrement(&lmap->ref_cnt);
    if(cur == NULL)
    {
        return NULL;
    }
    else
    {   
        return LFLISTMAP_LINK_TO_ENTRY(cur);
    }
}

void lflistmap_remove_internal(lflistmap_t *lmap, const void *key, void *entry)
{
    lflistmap_link_t * volatile prev;
    lflistmap_link_t * volatile cur;
    lflistmap_link_t *cur_next = NULL;

    InterlockedIncrement(&lmap->ref_cnt);
    prev = &lmap->list;
    cur = prev->next;
    for(;cur != NULL;)
    {
        cur_next = cur->next;
        
        if(((uintptr_t)cur_next & 1) == 0)
        {
            lflistmap_entry_t *cur_entry;
            int r_cmp;
            cur_entry = LFLISTMAP_LINK_TO_ENTRY(cur);
            r_cmp = lflistmap_key_cmp(&lmap->env, lflistmap_get_key(&lmap->env, cur_entry), key);
            
            if(r_cmp == 0)
            {
                if(LFLISTMAP_LINK_TO_ENTRY(cur) == entry || entry == NULL)
                {
                    InterlockedExchangePointer(&cur_entry->link.back_off, prev);
                    if(lmap->env.mark_value != NULL)
                    {
                        lmap->env.mark_value(cur_entry, TRUE);
                    }
                    cur_next = lflistmap_mark_link(&cur_entry->link);
                }
            }
            else if(r_cmp < 0)
            {
                prev = cur;
                cur = lflistmap_get_unmarked(cur_next);
                continue;
            }
            else
            {
                cur = NULL;
                break;
            }
        }
        
        if(((uintptr_t)cur_next & 1) != 0)
        {
            if(lflistmap_remove_marked_entry(lmap, cur, prev, cur_next))
            {
                cur = lflistmap_get_unmarked(cur_next);
            }
            else
            {
                lflistmap_retrace(&prev, &cur);
            }
        }
    }
    InterlockedDecrement(&lmap->ref_cnt);
}

void lflistmap_remove(lflistmap_t *lmap, const void *key)
{
    lflistmap_remove_internal(lmap, key, NULL);
    lflistmap_collect_garbage(lmap, FALSE);
}

BOOL lflistmap_exists(lflistmap_t *lmap, const void *key)
{
    lflistmap_entry_t *entry;
    entry = lflistmap_get_entry(lmap, key);
    if(entry != NULL)
    {
        lflistmap_release_entry(lmap, entry);
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

void lflistmap_remove_entry(lflistmap_t *lmap, lflistmap_entry_t *entry)
{
    void *key;
    key = lflistmap_get_key(&lmap->env, entry);
    lflistmap_remove_internal(lmap, key, entry);
}

void lflistmap_close(lflistmap_t *lmap)
{
    InterlockedCompareExchange(&lmap->close_flag, 1, 0);
    while(lmap->ref_cnt != 0)
    {
        revolve(1);
    }
}

lflistmap_entry_t *lflistmap_pop_entry(lflistmap_t *lmap)
{
    lflistmap_link_t *cur;
    lflistmap_link_t *cur_next;
    InterlockedIncrement(&lmap->ref_cnt);
    cur = lmap->list.next;
    while(cur != NULL)
    {
        cur_next = cur->next;
       
        if(((uintptr_t)cur_next & 1) == 0)
        {
            if(lmap->env.mark_value != NULL)
            {
                lmap->env.mark_value(LFLISTMAP_LINK_TO_ENTRY(cur), TRUE);
            }
            InterlockedExchangePointer(&cur->back_off, &lmap->list);
            cur_next = lflistmap_mark_link(cur);
        }
        
        if(((uintptr_t)cur_next & 1) != 0)
        {
            if(cur == InterlockedCompareExchangePointer(&lmap->list.next, lflistmap_get_unmarked(cur_next), cur))
            {
                InterlockedDecrement(&lmap->count);
                InterlockedDecrement(&LFLISTMAP_LINK_TO_ENTRY(cur)->ref_cnt);
                break;
            }
        }
        
        cur = lmap->list.next;
    }
    InterlockedDecrement(&lmap->ref_cnt);
    
    if(cur == NULL)
    {
        return NULL;
    }
    else
    {
        return LFLISTMAP_LINK_TO_ENTRY(cur);
    }
}

void lflistmap_clear(lflistmap_t *lmap, int close_flag)
{
    lflistmap_entry_t *entry;
    if(close_flag != 0)
    {
        lflistmap_close(lmap);
    }
    entry = lflistmap_pop_entry(lmap);
    while(entry != NULL)
    {
        lflistmap_try_free_entry(lmap, entry);
        entry = lflistmap_pop_entry(lmap);
    }
    lflistmap_collect_garbage(lmap, TRUE);
}
