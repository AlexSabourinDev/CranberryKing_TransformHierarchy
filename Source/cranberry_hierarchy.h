#ifndef __CRANBERRY_HIERARCHY_H
#define __CRANBERRY_HIERARCHY_H

#include "cranberry_math.h"

// #define CRANBERRY_HIERARCHY_IMPL to enable the implementation in a translation unit
// #define CRANBERRY_DEBUG to enable debug checks

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
void cranh_transform_locals_to_globals(cranh_hierarchy_t* hierarchy);

// @brief Reads the local transform addressed by handle
cranm_transform_t cranh_read_local(cranh_hierarchy_t* hierarchy, cranh_handle_t handle);
// @brief Write the transform to the location defined by handle
void cranh_write_local(cranh_hierarchy_t* hierarchy, cranh_handle_t handle, cranm_transform_t write);

// @brief Read a transform that has been transformed to the global coordinate space.
// WARNING: If a parent or the child transform has been modified and not converted to global space
// then the global transform will be stale.
cranm_transform_t cranh_read_global(cranh_hierarchy_t* hierarchy, cranh_handle_t transform);

// @brief Write a global transform to the location defined by the handle
void cranh_write_global(cranh_hierarchy_t* hierarchy, cranh_handle_t transform, cranm_transform_t write);

// IMPL

#ifdef CRANBERRY_HIERARCHY_IMPL

#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#ifdef CRANBERRY_DEBUG
	#include <assert.h>
#endif // CRANBERRY_DEBUG

typedef struct
{
	unsigned int maxTransformCount;
	unsigned int currentTransformCount;
	unsigned int dirtyStreamStart;
	unsigned int dirtyStreamEnd;
} cranh_header_t;

#define cranh_start_dirty_flag 0x02
#define cranh_start_bit_mask 0xAAAAAAAA
#define cranh_end_dirty_flag 0x01
#define cranh_end_bit_mask 0x55555555

// Buffer format:
// header
// local transforms [maxTransformCount]
// global transforms [maxTransformCount]
// max child index [maxTransformCount]
// dirty ranges [maxTransformCount / 4 + 1]

unsigned int cranh_buffer_size(unsigned int maxTransformCount)
{
	return 
		sizeof(cranh_header_t) +
		(sizeof(cranm_transform_t) +
		sizeof(cranm_transform_t) +
		sizeof(cranh_handle_t) + 
		sizeof(unsigned int)) * maxTransformCount + 
		sizeof(uint32_t) * (maxTransformCount / 16 + 1);
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
	memset(hierarchyHeader, 0, cranh_buffer_size(maxSize));
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
	bufferStart += sizeof(cranh_header_t) + sizeof(cranm_transform_t) * 2 * header->maxTransformCount;
	return (cranh_handle_t*)bufferStart + index;
}

// Max child index is the fourth buffer
unsigned int* cranh_get_max_child_index(cranh_hierarchy_t* hierarchy, unsigned int index)
{
	cranh_header_t* header = (cranh_header_t*)hierarchy;

	uint8_t* bufferStart = (uint8_t*)hierarchy;
	bufferStart += sizeof(cranh_header_t) + (sizeof(cranm_transform_t) * 2 + sizeof(cranh_handle_t)) * header->maxTransformCount;
	return (unsigned int*)bufferStart + index;
}

// Dirty stream is the fifth buffer
uint32_t* cranh_get_dirty_stream(cranh_hierarchy_t* hierarchy, unsigned int index)
{
	cranh_header_t* header = (cranh_header_t*)hierarchy;

	uint8_t* bufferStart = (uint8_t*)hierarchy;
	bufferStart += sizeof(cranh_header_t) + (sizeof(cranm_transform_t) * 2 + sizeof(cranh_handle_t) + sizeof(unsigned int)) * header->maxTransformCount;
	return (uint32_t*)bufferStart + index;
}

cranh_handle_t cranh_add(cranh_hierarchy_t* hierarchy, cranm_transform_t value)
{
	cranh_header_t* header = (cranh_header_t*)hierarchy;

#ifdef CRANBERRY_DEBUG
	assert(header->currentTransformCount < header->maxTransformCount);
#endif // CRANBERRY_DEBUG

	cranm_transform_t* local = cranh_get_local(hierarchy, header->currentTransformCount);
	cranm_transform_t* global = cranh_get_global(hierarchy, header->currentTransformCount);
	cranh_handle_t* parent = cranh_get_parent(hierarchy, header->currentTransformCount);

	parent->value = ~0;
	*global = value;
	*local = value;

	// dirty setup
	unsigned int* currentChildrenRange = cranh_get_max_child_index(hierarchy, header->currentTransformCount);
	*currentChildrenRange = header->currentTransformCount;

	return (cranh_handle_t){ .value = header->currentTransformCount++ };
}

cranh_handle_t cranh_add_with_parent(cranh_hierarchy_t* hierarchy, cranm_transform_t value, cranh_handle_t handle)
{
	cranh_header_t* header = (cranh_header_t*)hierarchy;

#ifdef CRANBERRY_DEBUG
	assert(header->currentTransformCount < header->maxTransformCount);
	assert(handle.value < header->currentTransformCount);
#endif // CRANBERRY_DEBUG

	cranm_transform_t* local = cranh_get_local(hierarchy, header->currentTransformCount);
	cranm_transform_t* global = cranh_get_global(hierarchy, header->currentTransformCount);
	cranh_handle_t* parent = cranh_get_parent(hierarchy, header->currentTransformCount);

	*parent = handle;
	*global = cranm_transform(value, *cranh_get_global(hierarchy, handle.value));
	*local = value;

	unsigned int* currentChildrenRange = cranh_get_max_child_index(hierarchy, header->currentTransformCount);
	*currentChildrenRange = header->currentTransformCount;

	unsigned int* parentChildrenRange = cranh_get_max_child_index(hierarchy, handle.value);

	// Our parent range should technically always be growing, never shrinking when adding a child
#ifdef CRANBERRY_DEBUG
	assert(*parentChildrenRange < header->currentTransformCount);
#endif // CRANBERRY_DEBUG

	// Our parent's range is now from our parent to us.
	*parentChildrenRange = header->currentTransformCount;

	return (cranh_handle_t){ .value = header->currentTransformCount++ };
}

cranm_transform_t cranh_read_local(cranh_hierarchy_t* hierarchy, cranh_handle_t transform)
{
#ifdef CRANBERRY_DEBUG
	cranh_header_t* header = (cranh_header_t*)hierarchy;
	assert(transform.value < header->currentTransformCount);
#endif // CRANBERRY_DEBUG

	return *cranh_get_local(hierarchy, transform.value);
}

void cranh_write_local(cranh_hierarchy_t* hierarchy, cranh_handle_t handle, cranm_transform_t write)
{
	cranh_header_t* header = (cranh_header_t*)hierarchy;

#ifdef CRANBERRY_DEBUG
	assert(handle.value < header->currentTransformCount);
#endif // CRANBERRY_DEBUG

	*cranh_get_local(hierarchy, handle.value) = write;

	uint32_t* rangeToggleStart = cranh_get_dirty_stream(hierarchy, handle.value / 16);
	*rangeToggleStart = *rangeToggleStart | (cranh_start_dirty_flag << (handle.value % 16 * 2));

	unsigned int maxChildIndex = *cranh_get_max_child_index(hierarchy, handle.value);
	uint32_t* rangeToggleEnd = cranh_get_dirty_stream(hierarchy, maxChildIndex / 16);
	*rangeToggleEnd = *rangeToggleEnd | (cranh_end_dirty_flag << (maxChildIndex % 16 * 2));

	header->dirtyStreamStart = handle.value < header->dirtyStreamStart ? handle.value : header->dirtyStreamStart;
	header->dirtyStreamEnd = maxChildIndex > header->dirtyStreamEnd ? maxChildIndex : header->dirtyStreamEnd;
}

cranm_transform_t cranh_read_global(cranh_hierarchy_t* hierarchy, cranh_handle_t transform)
{
#ifdef CRANBERRY_DEBUG
	cranh_header_t* header = (cranh_header_t*)hierarchy;
	assert(transform.value < header->currentTransformCount);
#endif // CRANBERRY_DEBUG

	return *cranh_get_global(hierarchy, transform.value);
}

void cranh_write_global(cranh_hierarchy_t* hierarchy, cranh_handle_t handle, cranm_transform_t write)
{
	cranh_header_t* header = (cranh_header_t*)hierarchy;

#ifdef CRANBERRY_DEBUG
	assert(handle.value < header->currentTransformCount);
#endif // CRANBERRY_DEBUG

	cranh_handle_t parentHandle = *cranh_get_parent(hierarchy, handle.value);
	if (parentHandle.value != ~0)
	{
		*cranh_get_local(hierarchy, handle.value) = cranm_inverse_transform(write, *cranh_get_global(hierarchy, parentHandle.value));
	}
	else
	{
		*cranh_get_local(hierarchy, handle.value) = write;
	}

	uint32_t* rangeToggleStart = cranh_get_dirty_stream(hierarchy, handle.value / 16);
	*rangeToggleStart = *rangeToggleStart | (cranh_start_dirty_flag << (handle.value % 16 * 2));

	unsigned int maxChildIndex = *cranh_get_max_child_index(hierarchy, handle.value);
	uint32_t* rangeToggleEnd = cranh_get_dirty_stream(hierarchy, maxChildIndex / 16);
	*rangeToggleEnd = *rangeToggleEnd | (cranh_end_dirty_flag << (maxChildIndex % 16 * 2));

	header->dirtyStreamStart = handle.value < header->dirtyStreamStart ? handle.value : header->dirtyStreamStart;
	header->dirtyStreamEnd = maxChildIndex > header->dirtyStreamEnd ? maxChildIndex : header->dirtyStreamEnd;
}

uint32_t cranh_bit_count(uint32_t i)
{
	i = ((i >> 1) & 0x55555555) + (i & 0x55555555);
	i = ((i >> 2) & 0x33333333) + (i & 0x33333333);
	i = ((i >> 4) & 0x0F0F0F0F) + (i & 0x0F0F0F0F);
	i = ((i >> 8) & 0x00FF00FF) + (i & 0x00FF00FF);
	i = (i >> 16) + (i & 0x0000FFFF);
	return i;
}

void cranh_transform_locals_to_globals(cranh_hierarchy_t* hierarchy)
{
	cranh_header_t* header = (cranh_header_t*)hierarchy;

	unsigned int start = header->dirtyStreamStart & ~15;
	unsigned int end = header->dirtyStreamEnd & ~15;

	cranm_transform_t* localIter = cranh_get_local(hierarchy, start);
	cranm_transform_t* globalIter = cranh_get_global(hierarchy, start);
	cranh_handle_t* parentIter = cranh_get_parent(hierarchy, start);
	uint32_t* dirtyStream = cranh_get_dirty_stream(hierarchy, start / 16);

#ifdef CRANBERRY_DEBUG
	unsigned int totalIndex = 0;
#endif // CRANBERRY_DEBUG

	// Stack start and ends
	unsigned int dirtyStreamStack = 0;
	for (unsigned int i = start; i < end + 1; i+=16, ++dirtyStream)
	{
		dirtyStreamStack += cranh_bit_count(*dirtyStream & cranh_start_bit_mask);

		if (dirtyStreamStack > 0)
		{
			for (unsigned int d = 0; d < 16; d++, ++localIter, ++globalIter, ++parentIter)
			{
#ifdef CRANBERRY_DEBUG
				++totalIndex;
#endif // CRANBERRY_DEBUG

				if (parentIter->value != ~0)
				{
#ifdef CRANBERRY_DEBUG
					assert(parentIter->value < totalIndex);
#endif // CRANBERRY_DEBUG

					cranm_transform_t* parent = cranh_get_global(hierarchy, parentIter->value);
					*globalIter = cranm_transform(*localIter, *parent);
				}
				else
				{
					*globalIter = *localIter;
				}
			}
		}

		dirtyStreamStack -= cranh_bit_count(*dirtyStream & cranh_end_bit_mask);
		*dirtyStream = 0;
	}

	header->dirtyStreamStart = 0;
	header->dirtyStreamEnd = 0;

#ifdef CRANBERRY_DEBUG
	assert(dirtyStreamStack == 0);
#endif // CRANBERRY_DEBUG
}


#endif // CRANBERRY_HIERARCHY_IMPL

#endif // __CRANBERRY_HIERARCHY_H
