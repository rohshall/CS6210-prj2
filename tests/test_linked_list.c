#include "CuTest.h"
#include <linked_list.c>

void test_ll_create(CuTest *tc)
{
	struct server_list_node *n = server_list_node_create();
	CuAssertIntEquals(tc, 0, n->has_work);
	CuAssertPtrEquals(tc, NULL, n->next);
	server_list_node_destroy(n);
}

void test_ll_init(CuTest *tc)
{
	struct server_list list;
	server_threads_list_init(&list);
	CuAssertPtrEquals(tc, NULL, list.first);
	CuAssertPtrEquals(tc, NULL, list.last);
}

void test_ll_insert(CuTest *tc)
{
	struct server_list list;
	server_threads_list_init(&list);

	struct server_list_node *n = server_list_node_create();
	server_list_insert(&list, n);
	CuAssertPtrEquals(tc, list.first, n);
	CuAssertPtrEquals(tc, list.last, n);
	CuAssertPtrEquals(tc, n->next, n);

	struct server_list_node *n2 = server_list_node_create();
	server_list_insert(&list, n2);
	CuAssertPtrEquals(tc, list.first, n2);
	CuAssertPtrEquals(tc, list.last, n);
	CuAssertPtrEquals(tc, n2->next, n);
	CuAssertPtrEquals(tc, n->next, n2);

	struct server_list_node *n3 = server_list_node_create();
	server_list_insert(&list, n3);
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
	SUITE_ADD_TEST(suite, test_ll_create);
	SUITE_ADD_TEST(suite, test_ll_init);
	SUITE_ADD_TEST(suite, test_ll_insert);

	return suite;
}

