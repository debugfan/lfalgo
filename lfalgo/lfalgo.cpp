// lfalgo.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "pattern_list.h"

void match_pattern_test(lflist_t *list, const char *text, void *expect)
{
	if (expect == match_pattern_list(list, text))
	{
		printf("Passed: %s\n", text);
	}
	else
	{
		printf("Not Passed: %s\n", text);
	}
}

int main()
{
	lflist_t pattern_list;

	init_pattern_list(&pattern_list);
	add_pattern_to_list(&pattern_list, "*.TXT", -1, (void *)1);
	add_pattern_to_list(&pattern_list, "*.TXT", -1, (void *)1);
	add_pattern_to_list(&pattern_list, "*.PDF", -1, (void *)1);
	add_pattern_to_list(&pattern_list, "*.XLS?", -1, (void *)1);
	match_pattern_test(&pattern_list, "ABC.TXT", (void *)1);
	match_pattern_test(&pattern_list, "ABC.XLS", (void *)0);
	remove_pattern_from_list(&pattern_list, "*.TXT");
	match_pattern_test(&pattern_list, "ABC.TXT", (void *)0);
	match_pattern_test(&pattern_list, "ABC.XLSX", (void *)1);
	clear_pattern_list(&pattern_list, FALSE);
	match_pattern_test(&pattern_list, "ABC.TXT", (void *)0);
	match_pattern_test(&pattern_list, "ABC.XLS", (void *)0);
	match_pattern_test(&pattern_list, "ABC.XLSX", (void *)0);
	system("pause");

    return 0;
}

