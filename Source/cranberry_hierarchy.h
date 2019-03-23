#ifndef __CRANBERRY_HIERARCHY_H
#define __CRANBERRY_HIERARCHY_H

#include "cranberry_math.h"

// #define CRANBERRY_HIERARCHY_IMPL to enable the implementation in a translation unit
// #define CRANBERRY_HIERARCHY_DEBUG to enable debug checks

typedef struct _cranh_hierarchy cranh_hierarchy;
typedef struct { unsigned int value; } cranh_handle;

// Create a cranh_hierarchy.
// WARNING: This function creates memory with malloc! It must be released with cranh_destroy.
cranh_hierarchy* cranh_create(unsigned int maxTransformCount);
void cranh_destroy(cranh_hierarchy* hierarchy);

unsigned int cranh_buffer_size(unsigned int maxTransformCount);
cranh_hierarchy* cranh_create_from_buffer(void* buffer, unsigned int maxTransformCount);

cranh_handle cranh_add(cranh_hierarchy* hierarchy, cran_mat4x4 value);
cranh_handle cranh_add_with_parent(cranh_hierarchy* hierarchy, cran_mat4x4 value, cranh_handle handle);
void cranh_transform_locals_to_globals(cranh_hierarchy* hierarchy);

cran_mat4x4 cranh_read_local(cranh_hierarchy* hierarchy, cranh_handle transform);
void cranh_write_local(cranh_hierarchy* hierarchy, cranh_handle transform, cran_mat4x4 write);

cran_mat4x4 cranh_read_global(cranh_hierarchy* hierarchy, cranh_handle transform);

#ifdef CRANBERRY_HIERARCHY_IMPL

#include <stdlib.h>
#include <stdint.h>

#ifdef CRANBERRY_HIERARCHY_DEBUG
	#include <assert.h>
#endif // CRANBERRY_HIERARCHY_DEBUG

typedef struct
{
	unsigned int maxTransformCount;
	unsigned int currentTransformCount;
} cranh_header;

unsigned int cranh_buffer_size(unsigned int maxTransformCount)
{
	return (sizeof(cran_mat4x4) * 2 + sizeof(cranh_handle)) * maxTransformCount + sizeof(cranh_header);
}

cranh_hierarchy* cranh_create(unsigned int maxTransformCount)
{
	unsigned int bufferSize = cranh_buffer_size(maxTransformCount);
	void* buffer = malloc(bufferSize);
	return cranh_create_from_buffer(buffer, maxTransformCount);
}

void cranh_destroy(cranh_hierarchy* hierarchy)
{
	free(hierarchy);
}

cranh_hierarchy* cranh_create_from_buffer(void* buffer, unsigned int maxSize)
{
	cranh_header* hierarchyHeader = (cranh_header*)buffer;
	hierarchyHeader->maxTransformCount = maxSize;
	hierarchyHeader->currentTransformCount = 0;
	return (cranh_hierarchy*)hierarchyHeader;
}

// Locals are the first buffer.
cran_mat4x4* cranh_get_local(cranh_hierarchy* hierarchy, unsigned int index)
{
	uint8_t* bufferStart = (uint8_t*)hierarchy;
	bufferStart += sizeof(cranh_header);
	return (cran_mat4x4*)bufferStart + index;
}

// Globals are the second buffer
cran_mat4x4* cranh_get_global(cranh_hierarchy* hierarchy, unsigned int index)
{
	cranh_header* header = (cranh_header*)hierarchy;

	uint8_t* bufferStart = (uint8_t*)hierarchy;
	bufferStart += sizeof(cranh_header) + sizeof(cran_mat4x4) * header->maxTransformCount;
	return (cran_mat4x4*)bufferStart + index;
}

// Indices are the third buffer
cranh_handle* cranh_get_parent(cranh_hierarchy* hierarchy, unsigned int index)
{
	cranh_header* header = (cranh_header*)hierarchy;

	uint8_t* bufferStart = (uint8_t*)hierarchy;
	bufferStart += sizeof(cranh_header) + sizeof(cran_mat4x4) * header->maxTransformCount * 2;
	return (cranh_handle*)bufferStart + index;
}

cranh_handle cranh_add(cranh_hierarchy* hierarchy, cran_mat4x4 value)
{
	cranh_header* header = (cranh_header*)hierarchy;

#ifdef CRANBERRY_HIERARCHY_DEBUG
	assert(header->currentTransformCount < header->maxTransformCount);
#endif // CRANBERRY_HIERARCHY_DEBUG

	cran_mat4x4* local = cranh_get_local(hierarchy, header->currentTransformCount);
	cranh_handle* parent = cranh_get_parent(hierarchy, header->currentTransformCount);

	parent->value = ~0;
	*local = value;

	return (cranh_handle){ .value = header->currentTransformCount++ };
}

cranh_handle cranh_add_with_parent(cranh_hierarchy* hierarchy, cran_mat4x4 value, cranh_handle handle)
{
	cranh_header* header = (cranh_header*)hierarchy;

#ifdef CRANBERRY_HIERARCHY_DEBUG
	assert(header->currentTransformCount < header->maxTransformCount);
	assert(handle.value < header->currentTransformCount);
#endif // CRANBERRY_HIERARCHY_DEBUG

	cran_mat4x4* local = cranh_get_local(hierarchy, header->currentTransformCount);
	cranh_handle* parent = cranh_get_parent(hierarchy, header->currentTransformCount);

	*parent = handle;
	*local = value;

	return (cranh_handle){ .value = header->currentTransformCount++ };
}

cran_mat4x4 cranh_read_local(cranh_hierarchy* hierarchy, cranh_handle transform)
{
#ifdef CRANBERRY_HIERARCHY_DEBUG
	cranh_header* header = (cranh_header*)hierarchy;
	assert(transform.value < header->currentTransformCount);
#endif // CRANBERRY_HIERARCHY_DEBUG

	return *cranh_get_local(hierarchy, transform.value);
}

void cranh_write_local(cranh_hierarchy* hierarchy, cranh_handle transform, cran_mat4x4 write)
{
#ifdef CRANBERRY_HIERARCHY_DEBUG
	cranh_header* header = (cranh_header*)hierarchy;
	assert(transform.value < header->currentTransformCount);
#endif // CRANBERRY_HIERARCHY_DEBUG

	*cranh_get_local(hierarchy, transform.value) = write;
}

cran_mat4x4 cranh_read_global(cranh_hierarchy* hierarchy, cranh_handle transform)
{
#ifdef CRANBERRY_HIERARCHY_DEBUG
	cranh_header* header = (cranh_header*)hierarchy;
	assert(transform.value < header->currentTransformCount);
#endif // CRANBERRY_HIERARCHY_DEBUG

	return *cranh_get_global(hierarchy, transform.value);
}

void cranh_transform_locals_to_globals(cranh_hierarchy* hierarchy)
{
	cranh_header* header = (cranh_header*)hierarchy;

	cran_mat4x4* localIter = cranh_get_local(hierarchy, 0);
	cran_mat4x4* globalIter = cranh_get_global(hierarchy, 0);
	cranh_handle* parentIter = cranh_get_parent(hierarchy, 0);
	for (unsigned int i = 0; i < header->currentTransformCount; ++i, ++localIter, ++globalIter, ++parentIter)
	{
		if (parentIter->value != ~0)
		{
#ifdef CRANBERRY_HIERARCHY_DEBUG
			assert(parentIter->value < i);
#endif // CRANBERRY_HIERARCHY_DEBUG

			cran_mat4x4* parent = cranh_get_global(hierarchy, parentIter->value);
			*globalIter = cran_mul4x4(*localIter, *parent);
		}
		else
		{
			*globalIter = *localIter;
		}
	}
}


#endif // CRANBERRY_HIERARCHY_IMPL

#endif // __CRANBERRY_HIERARCHY_H
