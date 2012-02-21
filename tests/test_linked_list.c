#include "CuTest.h"
#include <stlist.c>

void test_stlist_node_create(CuTest *tc)
{
	struct stlist_node *n = stlist_node_create();
	CuAssertIntEquals(tc, 0, n->has_work);
	CuAssertPtrEquals(tc, NULL, n->next);
	stlist_node_destroy(n);
}

void test_stlist_init(CuTest *tc)
{
	struct stlist list;
	stlist_init(&list);
	CuAssertPtrEquals(tc, NULL, list.first);
	CuAssertPtrEquals(tc, NULL, list.last);
}

void test_stlist_insert(CuTest *tc)
{
	struct stlist list;
	stlist_init(&list);

	struct stlist_node *n = stlist_node_create();
	stlist_insert(&list, n);
	CuAssertPtrEquals(tc, list.first, n);
	CuAssertPtrEquals(tc, list.last, n);
	CuAssertPtrEquals(tc, n->next, n);

	struct stlist_node *n2 = stlist_node_create();
	stlist_insert(&list, n2);
	CuAssertPtrEquals(tc, list.first, n2);
	CuAssertPtrEquals(tc, list.last, n);
	CuAssertPtrEquals(tc, n2->next, n);
	CuAssertPtrEquals(tc, n->next, n2);

	struct stlist_node *n3 = stlist_node_create();
	stlist_insert(&list, n3);
	CuAssertPtrEquals(tc, list.first, n3);
	CuAssertPtrEquals(tc, list.last, n);
	CuAssertPtrEquals(tc, n3->next, n2);
	CuAssertPtrEquals(tc, n2->next, n);
	CuAssertPtrEquals(tc, n->next, n3);
}

CuSuite* test_linked_list_get_suite()
{
	CuSuite *suite = CuSuiteNew();

	// SUITE_ADD_TEST calls
	SUITE_ADD_TEST(suite, test_stlist_node_create);
	SUITE_ADD_TEST(suite, test_stlist_init);
	SUITE_ADD_TEST(suite, test_stlist_insert);

	return suite;
}

