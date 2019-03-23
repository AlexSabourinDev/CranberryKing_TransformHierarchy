
#define CRANBERRY_HIERARCHY_IMPL
#define CRANBERRY_HIERARCHY_DEBUG
#include "cranberry_hierarchy.h"

#include "cranberry_math.h"

#include <stdio.h>

#define CRANBERRY_ENABLE_TESTS

#ifdef CRANBERRY_ENABLE_TESTS
#include <assert.h>
#include <string.h>

void test()
{
	cran_mat4x4 l = { 0 };
	l.m[0] = l.m[5] = l.m[10] = l.m[15] = 1.0f;
	l.m[3] = 5.0f;

	cran_mat4x4 r = { 0 };
	r.m[0] = r.m[5] = r.m[10] = r.m[15] = 3.0f;

	cran_mat4x4 rm = cran_mul4x4(l, r);

	cran_mat4x4 t = { 0 };
	t.m[0] = t.m[5] = t.m[10] = t.m[15] = 3.0f;
	t.m[3] = 15.0f;
	assert(memcmp(&rm, &t, sizeof(cran_mat4x4)) == 0);

	cranh_hierarchy* hierarchy = cranh_create(2);
	cranh_handle parent = cranh_add(hierarchy, l);
	cranh_handle child = cranh_add_with_parent(hierarchy, r, parent);

	cranh_transform_locals_to_globals(hierarchy);

	cran_mat4x4 childGlobal = cranh_read_global(hierarchy, child);
	assert(memcmp(&childGlobal, &t, sizeof(cran_mat4x4)) == 0);

	cranh_destroy(hierarchy);

}

#define cranberry_tests() test()

#else
#define cranberry_tests()
#endif // CRANBERRY_ENABLE_TESTS

int main()
{
	cranberry_tests();

	return 0;
}
