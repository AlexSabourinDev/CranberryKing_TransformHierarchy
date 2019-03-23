#ifndef __CRANBERRY_HIERARCHY_H
#define __CRANBERRY_HIERARCHY_H

#include "cranberry_math.h"

// #define CRANBERRY_HIERARCHY_IMPL to enable the implementation in a translation unit
// #define CRANBERRY_HIERARCHY_DEBUG to enable debug checks

// Types

typedef struct _cranh_hierarchy_t cranh_hierarchy_t;
typedef struct { unsigned int value; } cranh_handle_t;

// API

// @brief Create a cranh_hierarchy_t.
// @param maxTransformCount Determines the maximum number of transforms this hierarchy can support
// WARNING: This function allocates memory with the standard malloc. It must be released with cranh_destroy.
// If you want to allocate your own memory use the cranh_buffer... family of functions.
cranh_hierarchy_t* cranh_create(unsigned int maxTransformCount);
// Destroy the cranh_hierarchy created with cranh_create. This will also release the memory allocated by cranh_create.
void cranh_destroy(cranh_hierarchy_t* hierarchy);

// @brief Determines the minimum size necessary to allocate for a buffer.
// @param maxTransformCount Determines the maximum number of transforms this hierarchy can support
// Use this function in correspondance with @ref cranh_buffer_create to turn the buffer into a usable chunk of memory.
unsigned int cranh_buffer_size(unsigned int maxTransformCount);
// @brief Takes a buffer as input and initializes the memory into a workable chunk of memory for the remaining API calls.
// @param buffer An externally allocated chunk of memory of a minimum size of at least @ref cranh_buffer_size.
cranh_hierarchy_t* cranh_buffer_create(void* buffer, unsigned int maxTransformCount);

// @brief Add a transform to the hierarchy without a parent.
cranh_handle_t cranh_add(cranh_hierarchy_t* hierarchy, cranm_transform_t value);
// @brief Add a transform to the hierarchy with a specified parent.
cranh_handle_t cranh_add_with_parent(cranh_hierarchy_t* hierarchy, cranm_transform_t value, cranh_handle_t parent);
// @brief Transforms all locals to globals
void cranm_transform_locals_to_globals(cranh_hierarchy_t* hierarchy);

// @brief Reads the local transform addressed by handle
cranm_transform_t cranh_read_local(cranh_hierarchy_t* hierarchy, cranh_handle_t handle);
// @brief Write the transform to the location defined by handle
void cranh_write_local(cranh_hierarchy_t* hierarchy, cranh_handle_t handle, cranm_transform_t write);

// @brief Read a transform that has been transformed to the global coordinate space.
// WARNING: If a parent or the child transform has been modified and not converted to global space
// then the global transform will be stale.
cranm_transform_t cranh_read_global(cranh_hierarchy_t* hierarchy, cranh_handle_t transform);

// IMPL

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
} cranh_header_t;

unsigned int cranh_buffer_size(unsigned int maxTransformCount)
{
	return (sizeof(cranm_transform_t) * 2 + sizeof(cranh_handle_t)) * maxTransformCount + sizeof(cranh_header_t);
}

cranh_hierarchy_t* cranh_create(unsigned int maxTransformCount)
{
	unsigned int bufferSize = cranh_buffer_size(maxTransformCount);
	void* buffer = malloc(bufferSize);
	return cranh_buffer_create(buffer, maxTransformCount);
}

void cranh_destroy(cranh_hierarchy_t* hierarchy)
{
	free(hierarchy);
}

cranh_hierarchy_t* cranh_buffer_create(void* buffer, unsigned int maxSize)
{
	cranh_header_t* hierarchyHeader = (cranh_header_t*)buffer;
	hierarchyHeader->maxTransformCount = maxSize;
	hierarchyHeader->currentTransformCount = 0;
	return (cranh_hierarchy_t*)hierarchyHeader;
}

// Locals are the first buffer.
cranm_transform_t* cranh_get_local(cranh_hierarchy_t* hierarchy, unsigned int index)
{
	uint8_t* bufferStart = (uint8_t*)hierarchy;
	bufferStart += sizeof(cranh_header_t);
	return (cranm_transform_t*)bufferStart + index;
}

// Globals are the second buffer
cranm_transform_t* cranh_get_global(cranh_hierarchy_t* hierarchy, unsigned int index)
{
	cranh_header_t* header = (cranh_header_t*)hierarchy;

	uint8_t* bufferStart = (uint8_t*)hierarchy;
	bufferStart += sizeof(cranh_header_t) + sizeof(cranm_transform_t) * header->maxTransformCount;
	return (cranm_transform_t*)bufferStart + index;
}

// Indices are the third buffer
cranh_handle_t* cranh_get_parent(cranh_hierarchy_t* hierarchy, unsigned int index)
{
	cranh_header_t* header = (cranh_header_t*)hierarchy;

	uint8_t* bufferStart = (uint8_t*)hierarchy;
	bufferStart += sizeof(cranh_header_t) + sizeof(cranm_transform_t) * header->maxTransformCount * 2;
	return (cranh_handle_t*)bufferStart + index;
}

cranh_handle_t cranh_add(cranh_hierarchy_t* hierarchy, cranm_transform_t value)
{
	cranh_header_t* header = (cranh_header_t*)hierarchy;

#ifdef CRANBERRY_HIERARCHY_DEBUG
	assert(header->currentTransformCount < header->maxTransformCount);
#endif // CRANBERRY_HIERARCHY_DEBUG

	cranm_transform_t* local = cranh_get_local(hierarchy, header->currentTransformCount);
	cranh_handle_t* parent = cranh_get_parent(hierarchy, header->currentTransformCount);

	parent->value = ~0;
	*local = value;

	return (cranh_handle_t){ .value = header->currentTransformCount++ };
}

cranh_handle_t cranh_add_with_parent(cranh_hierarchy_t* hierarchy, cranm_transform_t value, cranh_handle_t handle)
{
	cranh_header_t* header = (cranh_header_t*)hierarchy;

#ifdef CRANBERRY_HIERARCHY_DEBUG
	assert(header->currentTransformCount < header->maxTransformCount);
	assert(handle.value < header->currentTransformCount);
#endif // CRANBERRY_HIERARCHY_DEBUG

	cranm_transform_t* local = cranh_get_local(hierarchy, header->currentTransformCount);
	cranh_handle_t* parent = cranh_get_parent(hierarchy, header->currentTransformCount);

	*parent = handle;
	*local = value;

	return (cranh_handle_t){ .value = header->currentTransformCount++ };
}

cranm_transform_t cranh_read_local(cranh_hierarchy_t* hierarchy, cranh_handle_t transform)
{
#ifdef CRANBERRY_HIERARCHY_DEBUG
	cranh_header_t* header = (cranh_header_t*)hierarchy;
	assert(transform.value < header->currentTransformCount);
#endif // CRANBERRY_HIERARCHY_DEBUG

	return *cranh_get_local(hierarchy, transform.value);
}

void cranh_write_local(cranh_hierarchy_t* hierarchy, cranh_handle_t transform, cranm_transform_t write)
{
#ifdef CRANBERRY_HIERARCHY_DEBUG
	cranh_header_t* header = (cranh_header_t*)hierarchy;
	assert(transform.value < header->currentTransformCount);
#endif // CRANBERRY_HIERARCHY_DEBUG

	*cranh_get_local(hierarchy, transform.value) = write;
}

cranm_transform_t cranh_read_global(cranh_hierarchy_t* hierarchy, cranh_handle_t transform)
{
#ifdef CRANBERRY_HIERARCHY_DEBUG
	cranh_header_t* header = (cranh_header_t*)hierarchy;
	assert(transform.value < header->currentTransformCount);
#endif // CRANBERRY_HIERARCHY_DEBUG

	return *cranh_get_global(hierarchy, transform.value);
}

void cranm_transform_locals_to_globals(cranh_hierarchy_t* hierarchy)
{
	cranh_header_t* header = (cranh_header_t*)hierarchy;

	cranm_transform_t* localIter = cranh_get_local(hierarchy, 0);
	cranm_transform_t* globalIter = cranh_get_global(hierarchy, 0);
	cranh_handle_t* parentIter = cranh_get_parent(hierarchy, 0);
	for (unsigned int i = 0; i < header->currentTransformCount; ++i, ++localIter, ++globalIter, ++parentIter)
	{
		if (parentIter->value != ~0)
		{
#ifdef CRANBERRY_HIERARCHY_DEBUG
			assert(parentIter->value < i);
#endif // CRANBERRY_HIERARCHY_DEBUG

			cranm_transform_t* parent = cranh_get_global(hierarchy, parentIter->value);
			*globalIter = cranm_transform(*localIter, *parent);
		}
		else
		{
			*globalIter = *localIter;
		}
	}
}


#endif // CRANBERRY_HIERARCHY_IMPL

#endif // __CRANBERRY_HIERARCHY_H
