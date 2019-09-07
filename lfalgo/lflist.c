#include "lflist.h"
#include <stdlib.h>
#include <string.h>

#define LFLIST_LINK_TO_ENTRY(link) ((lflist_entry_t *) link)

void lflist_init(lflist_t *lst)
{
    memset(lst, 0, sizeof(lflist_t));
}

__inline lflist_link_t * lflist_get_unmarked(lflist_link_t *p)
{
    if(((uintptr_t)p & 1) != 0)
    {
        return (lflist_link_t *)((uintptr_t)p ^ 1);
    }
    else
    {
        return p;
    }
}

void lflist_push_free_list(lflist_t *lst, lflist_entry_t *entry)
{
    lflist_link_t *head;
    lflist_link_t *last;
    
    if(entry != NULL)
    {
        for(last = &entry->link; last->free_next != NULL; last = last->free_next)
        {
            revolve(1);
        }
        
        for (;;) {
            head = lst->list.free_next;
            last->free_next = head;
            if (head == InterlockedCompareExchangePointer(&lst->list.free_next,
                entry,
                head))
            {
                break;
            }
        }
    }
}

void lflist_retrace(lflist_link_t * volatile *pprev, lflist_link_t *volatile *pcur)
{
    lflist_link_t * volatile prev_next;
    prev_next = (*pprev)->next;
    (*pcur) = lflist_get_unmarked(prev_next);
    while(((uintptr_t)prev_next & 1) != 0)
    {
        (*pprev) = (*pcur)->back_off;
        prev_next = (*pprev)->next;
        (*pcur) = lflist_get_unmarked(prev_next);
    }
}

void lflist_try_free_entry(lflist_t *lst, lflist_entry_t *entry, lflist_free_entry_func_t entry_free)
{
    if(lst->ref_cnt == 0 && entry->ref_cnt == 0)
    {
		entry_free(entry);
    }
    else
    {
        lflist_push_free_list(lst, entry);
    }    
}

static __inline BOOL lflist_remove_marked_entry(lflist_t *lst, lflist_link_t *cur, lflist_link_t *prev, lflist_link_t *cur_next)
{
    if(cur == InterlockedCompareExchangePointer(&prev->next, lflist_get_unmarked(cur_next), cur))
    {
        InterlockedDecrement(&lst->count);
        InterlockedDecrement(&LFLIST_LINK_TO_ENTRY(cur)->ref_cnt);
        lflist_push_free_list(lst, LFLIST_LINK_TO_ENTRY(cur));
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

BOOL lflist_insert_entry_internal(lflist_t *lst, lflist_entry_t *neo, lflist_link_t *prev, lflist_link_t *next)
{
    neo->link.next = next;
    if (next == InterlockedCompareExchangePointer(&prev->next,
        &neo->link.next,
        next))
    {
        InterlockedIncrement(&lst->count);
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

BOOL lflist_add_internal(lflist_t *lst, lflist_entry_t *neo, lflist_entry_t **added_entry, 
	lflist_entry_cmp_func_t entry_cmp, void *condi, 
	lflist_update_func_t entry_update, void *data)
{
    lflist_link_t * volatile prev;
    lflist_link_t * volatile cur;
    lflist_link_t * cur_next;
    BOOL r = FALSE;
    
    if(neo != NULL)
    {
        if(added_entry != NULL)
        {
            InterlockedIncrement(&neo->ref_cnt);
        }
        prev = &lst->list;
        cur = prev->next;
        for(;;)
        {
            if(cur == NULL)
            {
                if(lflist_insert_entry_internal(lst, neo, prev, cur))
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
                    lflist_retrace(&prev, &cur);
                    continue;
                }
            }
            
            cur_next = cur->next;
            
            if(((uintptr_t)cur_next & 1) == 0)
            {
                int r_cmp;
				if (entry_cmp != NULL) {
					r_cmp = entry_cmp(LFLIST_LINK_TO_ENTRY(cur), condi);
				}
				else {
					r_cmp = (int)(intptr_t)condi;
				}
                if(r_cmp < 0)
                {
                    prev = cur;
                    cur = cur_next;
                    continue;
                }
                else if(r_cmp == 0)
                {
                    lflist_entry_t *cur_entry;
                    BOOL retry = FALSE;
                    cur_entry = LFLIST_LINK_TO_ENTRY(cur);
                    InterlockedIncrement(&cur_entry->ref_cnt);
					if(entry_update != NULL && entry_update(cur_entry, data, &retry))
                    {
                        if(added_entry != NULL)
                        {
                            *added_entry = cur_entry;
                        }
                        else
                        {
                            InterlockedDecrement(&cur_entry->ref_cnt);
                        }
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
                            r = FALSE;
                            break;
                        }
                    }
                }
                else
                {
                    if(lflist_insert_entry_internal(lst, neo, prev, cur))
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
                        lflist_retrace(&prev, &cur);
                        continue;
                    }
                }
            }
            
            if(((uintptr_t)cur_next & 1) != 0)
            {
                if(lflist_remove_marked_entry(lst, cur, prev, cur_next))
                {
                    cur = lflist_get_unmarked(cur_next);
                }
                else
                {
                    lflist_retrace(&prev, &cur);
                }
            }
        }
    }
    return r;
}

BOOL lflist_add_entry(lflist_t *lst, lflist_entry_t *entry)
{
	BOOL r = FALSE;

	if (lst->close_flag == 0)
	{
		InterlockedIncrement(&lst->ref_cnt);
		if (lst->close_flag == 0)
		{
			r = lflist_add_internal(lst, entry, NULL, NULL, (void *)(intptr_t)-1, NULL, NULL);
		}
		InterlockedDecrement(&lst->ref_cnt);
	}

	return r;
}

BOOL lflist_insert_entry(lflist_t *lst, lflist_entry_t *entry)
{
	BOOL r = FALSE;

	if (lst->close_flag == 0)
	{
		InterlockedIncrement(&lst->ref_cnt);
		if (lst->close_flag == 0)
		{
			r = lflist_add_internal(lst, entry, NULL, NULL, (void *)(long)1, NULL, NULL);
		}
		InterlockedDecrement(&lst->ref_cnt);
	}

	return r;
}

void lflist_collect_garbage(lflist_t *lst, BOOL force_collect, lflist_free_entry_func_t entry_free)
{
    lflist_link_t *head;
    lflist_link_t *link;
    lflist_link_t *volatile *prev_next_p;
    link = InterlockedExchangePointer(&lst->list.free_next, NULL);
    if(link != NULL)
    {
        if(lst->ref_cnt != 0)
        {
            if(force_collect)
            {
                while(lst->ref_cnt > 0)
                {
                    revolve(1);
                }
            }
            else
            {                
                lflist_push_free_list(lst, LFLIST_LINK_TO_ENTRY(link));
                return;
            }
        }
        
        head = link;
        prev_next_p = &head;
        while(link != NULL)
        {
            lflist_link_t *next;
            next = link->free_next;
            
            if(force_collect)
            {
                while(LFLIST_LINK_TO_ENTRY(link)->ref_cnt > 0)
                {
                    revolve(1);
                }
            }
            
            if(LFLIST_LINK_TO_ENTRY(link)->ref_cnt > 0)
            {
                prev_next_p = &link->free_next;
                link = next;
            }
            else
            {
                *prev_next_p = next;
				if (entry_free != NULL)
				{
					entry_free(LFLIST_LINK_TO_ENTRY(link));
				}
                link = next;
            }
        }
        lflist_push_free_list(lst, LFLIST_LINK_TO_ENTRY(head));
    }
}

void lflist_release_entry(lflist_t *lst, lflist_entry_t *entry, lflist_free_entry_func_t entry_free)
{
    if(0 == InterlockedDecrement(&entry->ref_cnt))
    {
        lflist_collect_garbage(lst, FALSE, entry_free);
    }
}

lflist_link_t *lflist_mark_link(lflist_link_t *link)
{
    lflist_link_t *next;
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

BOOL lflist_traverse(lflist_t *lst, lflist_traverse_callback_t callback, const void *in_data, void *out_data)
{
    lflist_link_t * volatile prev;
    lflist_link_t * volatile cur;
    lflist_link_t *cur_next = NULL;
	BOOL ret = TRUE;

    InterlockedIncrement(&lst->ref_cnt);
    prev = &lst->list;
    cur = prev->next;
    for(;cur != NULL;)
    {
        cur_next = cur->next;
        
        if(((uintptr_t)cur_next & 1) == 0)
        {
            if(!callback(LFLIST_LINK_TO_ENTRY(cur), in_data, out_data))
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
            if(lflist_remove_marked_entry(lst, cur, prev, cur_next))
            {
                cur = lflist_get_unmarked(cur_next);
            }
            else
            {
                lflist_retrace(&prev, &cur);
            }
        }
    }
    InterlockedDecrement(&lst->ref_cnt);
	return ret;
}

typedef struct {
	lflist_entry_cmp_func_t func;
	void *condi;
} lflist_exists_callback_data_t;

BOOL lflist_exists_callback(lflist_entry_t *entry, const void *in_data, void *out_data)
{
	const lflist_exists_callback_data_t *cbd = in_data;
	if (cbd != NULL && cbd->func != NULL) {
		if (0 == cbd->func(entry, cbd->condi)) {
			*(BOOL *)out_data = 1;
			return FALSE;
		}
		else {
			return TRUE;
		}
	}
	else {
		return TRUE;
	}
}

BOOL lflist_exists(lflist_t *lst, lflist_entry_cmp_func_t cmp_func, void *condi)
{
	lflist_exists_callback_data_t cbd;
	BOOL result = 0;
	cbd.func = cmp_func;
	cbd.condi = condi;
	lflist_traverse(lst, lflist_exists_callback, &cbd, (void *)(intptr_t)&result);
	return result;
}

lflist_entry_t *lflist_get_entry(lflist_t *lst, lflist_entry_cmp_func_t func, void *condi)
{
    lflist_link_t * volatile prev;
    lflist_link_t * volatile cur;
    lflist_link_t *cur_next = NULL;

    InterlockedIncrement(&lst->ref_cnt);
    prev = &lst->list;
    cur = prev->next;
    for(;cur != NULL;)
    {
        cur_next = cur->next;
        
        if(((uintptr_t)cur_next & 1) == 0)
        {
            lflist_entry_t *cur_entry;
            int r_cmp;
            
            cur_entry = LFLIST_LINK_TO_ENTRY(cur);
			if (func != NULL)
			{
				r_cmp = func(cur_entry, condi);
			}
			else
			{
				r_cmp = (cur_entry == condi ? 0 : -1);
			}
                        
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
                cur = lflist_get_unmarked(cur_next);
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
            if(lflist_remove_marked_entry(lst, cur, prev, cur_next))
            {
                cur = lflist_get_unmarked(cur_next);
            }
            else
            {
                lflist_retrace(&prev, &cur);
            }
        }
    }
    InterlockedDecrement(&lst->ref_cnt);
    if(cur == NULL)
    {
        return NULL;
    }
    else
    {   
        return LFLIST_LINK_TO_ENTRY(cur);
    }
}

void lflist_remove_internal(lflist_t *lst, lflist_entry_cmp_func_t func, const void *condi, lflist_smear_func_t smear_func)
{
    lflist_link_t * volatile prev;
    lflist_link_t * volatile cur;
    lflist_link_t *cur_next = NULL;

    InterlockedIncrement(&lst->ref_cnt);
    prev = &lst->list;
    cur = prev->next;
    for(;cur != NULL;)
    {
        cur_next = cur->next;
        
        if(((uintptr_t)cur_next & 1) == 0)
        {
            lflist_entry_t *cur_entry;
            int r_cmp;
            cur_entry = LFLIST_LINK_TO_ENTRY(cur);
			if (func != NULL) 
			{
				r_cmp = func(cur_entry, condi);
			}
			else 
			{
				r_cmp = (cur_entry == condi ? 0 : -1);
			}
            
            if(r_cmp == 0)
            {
                InterlockedExchangePointer(&cur_entry->link.back_off, prev);
                if(smear_func != NULL)
                {
					smear_func(cur_entry, TRUE);
                }
                cur_next = lflist_mark_link(&cur_entry->link);
            }
            else if(r_cmp < 0)
            {
                prev = cur;
                cur = lflist_get_unmarked(cur_next);
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
            if(lflist_remove_marked_entry(lst, cur, prev, cur_next))
            {
                cur = lflist_get_unmarked(cur_next);
            }
            else
            {
                lflist_retrace(&prev, &cur);
            }
        }
    }
    InterlockedDecrement(&lst->ref_cnt);
}

void lflist_remove(lflist_t *lst, lflist_entry_cmp_func_t func, const void *condi, lflist_free_entry_func_t entry_free)
{
    lflist_remove_internal(lst, func, condi, NULL);
    lflist_collect_garbage(lst, FALSE, entry_free);
}

void lflist_remove_entry(lflist_t *lst, lflist_entry_t *entry, lflist_smear_func_t smear_func)
{
    lflist_remove_internal(lst, NULL, entry, smear_func);
}

void lflist_close(lflist_t *lst)
{
    InterlockedCompareExchange(&lst->close_flag, 1, 0);
    while(lst->ref_cnt != 0)
    {
        revolve(1);
    }
}

lflist_entry_t *lflist_pop_entry(lflist_t *lst, lflist_smear_func_t smear_func)
{
    lflist_link_t *cur;
    lflist_link_t *cur_next;
    InterlockedIncrement(&lst->ref_cnt);
    cur = lst->list.next;
    while(cur != NULL)
    {
        cur_next = cur->next;
       
        if(((uintptr_t)cur_next & 1) == 0)
        {
            if(smear_func != NULL)
            {
				smear_func(LFLIST_LINK_TO_ENTRY(cur), TRUE);
            }
            InterlockedExchangePointer(&cur->back_off, &lst->list);
            cur_next = lflist_mark_link(cur);
        }
        
        if(((uintptr_t)cur_next & 1) != 0)
        {
            if(cur == InterlockedCompareExchangePointer(&lst->list.next, lflist_get_unmarked(cur_next), cur))
            {
                InterlockedDecrement(&lst->count);
                InterlockedDecrement(&LFLIST_LINK_TO_ENTRY(cur)->ref_cnt);
                break;
            }
        }
        
        cur = lst->list.next;
    }
    InterlockedDecrement(&lst->ref_cnt);
    
    if(cur == NULL)
    {
        return NULL;
    }
    else
    {
        return LFLIST_LINK_TO_ENTRY(cur);
    }
}

void lflist_clear(lflist_t *lst, lflist_free_entry_func_t entry_free, int close_flag)
{
    lflist_entry_t *entry;
    if(close_flag != 0)
    {
        lflist_close(lst);
    }
    entry = lflist_pop_entry(lst, NULL);
    while(entry != NULL)
    {
        lflist_try_free_entry(lst, entry, entry_free);
        entry = lflist_pop_entry(lst, NULL);
    }
    lflist_collect_garbage(lst, TRUE, entry_free);
}
