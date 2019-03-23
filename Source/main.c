
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
	cranm_transform_t c = { .pos = {.x = 5.0f,.y = 0.0f,.z = 0.0f},.rot = {0},.scale = {.x = 1.0f,.y = 1.0f,.z = 1.0f} };

	cranm_transform_t p = { .pos = {.x = 5.0f,.y = 0.0f,.z = 0.0f},.rot = {0},.scale = {.x = 5.0f,.y = 5.0f,.z = 5.0f} };

	cranm_transform_t rt = cranm_transform(c, p);

	cranm_transform_t t = { .pos = {.x = 30.0f,.y = 0.0f,.z = 0.0f},.rot = {0},.scale = {.x = 5.0f,.y = 5.0f,.z = 5.0f} };
	assert(memcmp(&rt, &t, sizeof(cranm_transform_t)) == 0);

	cranh_hierarchy_t* hierarchy = cranh_create(2);
	cranh_handle_t parent = cranh_add(hierarchy, p);
	cranh_handle_t child = cranh_add_with_parent(hierarchy, c, parent);

	cranm_transform_locals_to_globals(hierarchy);

	cranm_transform_t childGlobal = cranh_read_global(hierarchy, child);
	assert(memcmp(&childGlobal, &t, sizeof(cranm_transform_t)) == 0);

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
