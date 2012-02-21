#include "CuTest.h"
#include <linked_list.c>

void test_ll_create(CuTest *tc)
{
	CuFail(tc, "Not yet implemented");
}

CuSuite* test_linked_list_get_suite()
{
	CuSuite *suite = CuSuiteNew();

	// SUITE_ADD_TEST calls
	SUITE_ADD_TEST(suite, test_ll_create);

	return suite;
}

