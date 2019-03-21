#ifndef __CRANBERRY_HIERARCHY_H
#define __CRANBERRY_HIERARCHY_H

#include "cranberry_math.h"

// #define CRANBERRY_HIERARCHY_IMPL to enable the implementation in a translation unit
// #define CRANBERRY_HIERARCHY_DEBUG to enable debug checks

typedef struct _cran_hierarchy cran_hierarchy;
typedef struct { unsigned int value; } cran_hierarchy_handle;

// Create a cran_hierarchy.
// WARNING: This function creates memory with malloc! It must be released with cran_hierarchy_destroy.
cran_hierarchy* cran_hierarchy_create(unsigned int maxTransformCount);
void cran_hierarchy_destroy(cran_hierarchy* hierarchy);

unsigned int cran_hierarchy_buffer_size(unsigned int maxTransformCount);
cran_hierarchy* cran_hierarchy_create_from_buffer(void* buffer, unsigned int maxTransformCount);

cran_hierarchy_handle cran_hierarchy_add(cran_hierarchy* hierarchy, cran_mat4x4 value);
cran_hierarchy_handle cran_hierarchy_add_with_parent(cran_hierarchy* hierarchy, cran_mat4x4 value, cran_hierarchy_handle handle);
void cran_hierarchy_transform_locals_to_globals(cran_hierarchy* hierarchy);

cran_mat4x4 cran_hierarchy_read_local(cran_hierarchy* hierarchy, cran_hierarchy_handle transform);
void cran_hierarchy_write_local(cran_hierarchy* hierarchy, cran_hierarchy_handle transform, cran_mat4x4 write);

cran_mat4x4 cran_hierarchy_read_global(cran_hierarchy* hierarchy, cran_hierarchy_handle transform);

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
} cran_hierarchy_header;

unsigned int cran_hierarchy_buffer_size(unsigned int maxTransformCount)
{
	return (sizeof(cran_mat4x4) * 2 + sizeof(cran_hierarchy_handle)) * maxTransformCount + sizeof(cran_hierarchy_header);
}

cran_hierarchy* cran_hierarchy_create(unsigned int maxTransformCount)
{
	unsigned int bufferSize = cran_hierarchy_buffer_size(maxTransformCount);
	void* buffer = malloc(bufferSize);
	return cran_hierarchy_create_from_buffer(buffer, maxTransformCount);
}

void cran_hierarchy_destroy(cran_hierarchy* hierarchy)
{
	free(hierarchy);
}

cran_hierarchy* cran_hierarchy_create_from_buffer(void* buffer, unsigned int maxSize)
{
	cran_hierarchy_header* hierarchyHeader = (cran_hierarchy_header*)buffer;
	hierarchyHeader->maxTransformCount = maxSize;
	hierarchyHeader->currentTransformCount = 0;
	return (cran_hierarchy*)hierarchyHeader;
}

// Locals are the first buffer.
cran_mat4x4* cran_hierarchy_get_local(cran_hierarchy* hierarchy, unsigned int index)
{
	uint8_t* bufferStart = (uint8_t*)hierarchy;
	bufferStart += sizeof(cran_hierarchy_header);
	return (cran_mat4x4*)bufferStart + index;
}

// Globals are the second buffer
cran_mat4x4* cran_hierarchy_get_global(cran_hierarchy* hierarchy, unsigned int index)
{
	cran_hierarchy_header* header = (cran_hierarchy_header*)hierarchy;

	uint8_t* bufferStart = (uint8_t*)hierarchy;
	bufferStart += sizeof(cran_hierarchy_header) + sizeof(cran_mat4x4) * header->maxTransformCount;
	return (cran_mat4x4*)bufferStart + index;
}

// Indices are the third buffer
cran_hierarchy_handle* cran_hierarchy_get_parent(cran_hierarchy* hierarchy, unsigned int index)
{
	cran_hierarchy_header* header = (cran_hierarchy_header*)hierarchy;

	uint8_t* bufferStart = (uint8_t*)hierarchy;
	bufferStart += sizeof(cran_hierarchy_header) + sizeof(cran_mat4x4) * header->maxTransformCount * 2;
	return (cran_hierarchy_handle*)bufferStart + index;
}

cran_hierarchy_handle cran_hierarchy_add(cran_hierarchy* hierarchy, cran_mat4x4 value)
{
	cran_hierarchy_header* header = (cran_hierarchy_header*)hierarchy;

#ifdef CRANBERRY_HIERARCHY_DEBUG
	assert(header->currentTransformCount < header->maxTransformCount);
#endif // CRANBERRY_HIERARCHY_DEBUG

	cran_mat4x4* local = cran_hierarchy_get_local(hierarchy, header->currentTransformCount);
	cran_hierarchy_handle* parent = cran_hierarchy_get_parent(hierarchy, header->currentTransformCount);

	parent->value = ~0;
	*local = value;

	return (cran_hierarchy_handle){ .value = header->currentTransformCount++ };
}

cran_hierarchy_handle cran_hierarchy_add_with_parent(cran_hierarchy* hierarchy, cran_mat4x4 value, cran_hierarchy_handle handle)
{
	cran_hierarchy_header* header = (cran_hierarchy_header*)hierarchy;

#ifdef CRANBERRY_HIERARCHY_DEBUG
	assert(header->currentTransformCount < header->maxTransformCount);
	assert(handle.value < header->currentTransformCount);
#endif // CRANBERRY_HIERARCHY_DEBUG

	cran_mat4x4* local = cran_hierarchy_get_local(hierarchy, header->currentTransformCount);
	cran_hierarchy_handle* parent = cran_hierarchy_get_parent(hierarchy, header->currentTransformCount);

	*parent = handle;
	*local = value;

	return (cran_hierarchy_handle){ .value = header->currentTransformCount++ };
}

cran_mat4x4 cran_hierarchy_read_local(cran_hierarchy* hierarchy, cran_hierarchy_handle transform)
{
#ifdef CRANBERRY_HIERARCHY_DEBUG
	cran_hierarchy_header* header = (cran_hierarchy_header*)hierarchy;
	assert(transform.value < header->currentTransformCount);
#endif // CRANBERRY_HIERARCHY_DEBUG

	return *cran_hierarchy_get_local(hierarchy, transform.value);
}

void cran_hierarchy_write_local(cran_hierarchy* hierarchy, cran_hierarchy_handle transform, cran_mat4x4 write)
{
#ifdef CRANBERRY_HIERARCHY_DEBUG
	cran_hierarchy_header* header = (cran_hierarchy_header*)hierarchy;
	assert(transform.value < header->currentTransformCount);
#endif // CRANBERRY_HIERARCHY_DEBUG

	*cran_hierarchy_get_local(hierarchy, transform.value) = write;
}

cran_mat4x4 cran_hierarchy_read_global(cran_hierarchy* hierarchy, cran_hierarchy_handle transform)
{
#ifdef CRANBERRY_HIERARCHY_DEBUG
	cran_hierarchy_header* header = (cran_hierarchy_header*)hierarchy;
	assert(transform.value < header->currentTransformCount);
#endif // CRANBERRY_HIERARCHY_DEBUG

	return *cran_hierarchy_get_global(hierarchy, transform.value);
}

void cran_hierarchy_transform_locals_to_globals(cran_hierarchy* hierarchy)
{
	cran_hierarchy_header* header = (cran_hierarchy_header*)hierarchy;

	cran_mat4x4* localIter = cran_hierarchy_get_local(hierarchy, 0);
	cran_mat4x4* globalIter = cran_hierarchy_get_global(hierarchy, 0);
	cran_hierarchy_handle* parentIter = cran_hierarchy_get_parent(hierarchy, 0);
	for (unsigned int i = 0; i < header->currentTransformCount; ++i, ++localIter, ++globalIter, ++parentIter)
	{
		if (parentIter->value != ~0)
		{
#ifdef CRANBERRY_HIERARCHY_DEBUG
			assert(parentIter->value < i);
#endif // CRANBERRY_HIERARCHY_DEBUG

			cran_mat4x4* parent = cran_hierarchy_get_global(hierarchy, parentIter->value);
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
