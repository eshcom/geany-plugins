/*
 * 
 */

#include <stdlib.h>
#include <stdio.h>
#include <check.h>

#include <gtkcompat.h>
#include <geany.h>


Suite *my_suite(void)
{
	Suite *s = suite_create("GeanyDoc");
	return s;
}

int main(void)
{
	Suite *s = my_suite();
	SRunner *sr = srunner_create(s);
	srunner_run_all(sr, CK_NORMAL);
	
	int nf = srunner_ntests_failed(sr);
	srunner_free(sr);
	
	return (nf == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
