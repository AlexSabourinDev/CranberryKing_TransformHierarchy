#ifndef __CRANBERRY_HIERARCHY_H
#define __CRANBERRY_HIERARCHY_H

#include "cranberry_math.h"

//
// cranberry_hierarchy.h
// @brief Cranberry hierarchy is a simple transform hierarchy focused on efficiency and simplicity.
// WARNING: This hierarchy expects that the transforms read from cranh_read_local and cranh_read_global are from
// the previous step and only advanced once cranh_transform_locals_to_globals is called.
// This decision was made to allow for easier debugging and reasoning. Instead of trying to reason about the order of transform modifications,
// you work with a fixed snapshot every frame.
// Say, you were put in a position where various logic components reads and writes their transform in an arbitrary order, then it might be unclear
// if you're running before your parent's position has been updated or not. If the child is run before the parent, the position used for it's logic
// will be different than if the parent's position had been updated. As a result, defining a fixed point where all transforms are updated allows the
// user to reason about the functionality a lot more easily. It also takes the time spent updating transforms out of the logic code and providing 
// a more consistent profiling report.
//

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


cranh_handle_t cranh_add(cranh_hierarchy_t* hierarchy, cranm_transform_t value);
cranh_handle_t cranh_add_with_parent(cranh_hierarchy_t* hierarchy, cranm_transform_t value, cranh_handle_t parent);

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
#include <stdbool.h>

#ifdef CRANBERRY_DEBUG
	#include <assert.h>
#endif // CRANBERRY_DEBUG

#define cranh_dirty_start_flag 0x02
#define cranh_dirty_start_bit_mask 0xAA
#define cranh_dirty_end_flag 0x01
#define cranh_dirty_end_bit_mask 0x55
#define cranh_invalid_handle ~0

typedef struct
{
	unsigned int start;
	unsigned int end;
} cranh_dirty_scheme_header_t;

unsigned int cranh_dirty_scheme_size(unsigned int maxTransformCount)
{
	return sizeof(cranh_dirty_scheme_header_t) + sizeof(uint32_t) * (maxTransformCount / 16 + 1);
}

void cranh_dirty_add_interval(cranh_dirty_scheme_header_t* intervalSetHeader, unsigned int min, unsigned int max)
{
#ifdef CRANBERRY_DEBUG
	assert(min <= max);
#endif // CRANBERRY_DEBUG

	uint32_t* dirtyStream = (uint32_t*)(intervalSetHeader + 1);
	uint32_t* dirtyStart = dirtyStream + (min >> 4);
	*dirtyStart = *dirtyStart | (cranh_dirty_start_flag << ((min & 0x0F) << 1));

	uint32_t* dirtyEnd = dirtyStream + (max >> 4);
	*dirtyEnd = *dirtyEnd | (cranh_dirty_end_flag << ((max & 0x0F) << 1));

	intervalSetHeader->start = min < intervalSetHeader->start ? min & ~0x03 : intervalSetHeader->start;
	intervalSetHeader->end = max > intervalSetHeader->end ? max & ~0x03 : intervalSetHeader->end;
}

typedef struct
{
	unsigned int maxTransformCount;
	unsigned int currentTransformCount;
} cranh_header_t;

// Buffer format:
// header
// local transforms [maxTransformCount]
// global transforms [maxTransformCount]
// max child index [maxTransformCount]
// dirty scheme

unsigned int cranh_buffer_size(unsigned int maxTransformCount)
{
	return 
		sizeof(cranh_header_t) +
		(sizeof(cranm_transform_t) +
		sizeof(cranm_transform_t) +
		sizeof(cranh_handle_t) + 
		sizeof(unsigned int)) * maxTransformCount + 
		cranh_dirty_scheme_size(maxTransformCount);
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

// Dirty scheme is the fifth buffer
cranh_dirty_scheme_header_t* cranh_get_dirty_scheme(cranh_hierarchy_t* hierarchy)
{
	cranh_header_t* header = (cranh_header_t*)hierarchy;

	uint8_t* bufferStart = (uint8_t*)hierarchy;
	bufferStart += sizeof(cranh_header_t) + (sizeof(cranm_transform_t) * 2 + sizeof(cranh_handle_t) + sizeof(unsigned int)) * header->maxTransformCount;
	return (cranh_dirty_scheme_header_t*)bufferStart;
}

cranh_handle_t cranh_add(cranh_hierarchy_t* hierarchy, cranm_transform_t value)
{
	cranh_header_t* header = (cranh_header_t*)hierarchy;
	unsigned int transformHandle = header->currentTransformCount;
	++header->currentTransformCount;

#ifdef CRANBERRY_DEBUG
	assert(transformHandle < header->maxTransformCount);
#endif // CRANBERRY_DEBUG

	cranm_transform_t* local = cranh_get_local(hierarchy, transformHandle);
	cranm_transform_t* global = cranh_get_global(hierarchy, transformHandle);
	cranh_handle_t* parent = cranh_get_parent(hierarchy, transformHandle);

	parent->value = cranh_invalid_handle;
	*global = value;
	*local = value;

	// dirty setup
	unsigned int* currentChildrenRange = cranh_get_max_child_index(hierarchy, transformHandle);
	*currentChildrenRange = transformHandle;

	return (cranh_handle_t){ .value = transformHandle };
}

cranh_handle_t cranh_add_with_parent(cranh_hierarchy_t* hierarchy, cranm_transform_t value, cranh_handle_t parentHandle)
{
	cranh_header_t* header = (cranh_header_t*)hierarchy;
	unsigned int transformHandle = header->currentTransformCount;
	++header->currentTransformCount;

#ifdef CRANBERRY_DEBUG
	assert(transformHandle < header->maxTransformCount);
	assert(parentHandle.value < transformHandle);
#endif // CRANBERRY_DEBUG

	cranm_transform_t* local = cranh_get_local(hierarchy, transformHandle);
	cranm_transform_t* global = cranh_get_global(hierarchy, transformHandle);
	cranh_handle_t* parent = cranh_get_parent(hierarchy, transformHandle);

	*parent = parentHandle;
	*global = cranm_transform(value, *cranh_get_global(hierarchy, parentHandle.value));
	*local = value;

	unsigned int* currentChildrenRange = cranh_get_max_child_index(hierarchy, transformHandle);
	*currentChildrenRange = transformHandle;

	// Update all of the parents
	while (parentHandle.value != cranh_invalid_handle)
	{
		unsigned int* parentChildrenRange = cranh_get_max_child_index(hierarchy, parentHandle.value);
		*parentChildrenRange = *parentChildrenRange < transformHandle ? transformHandle : *parentChildrenRange;
		parentHandle = *cranh_get_parent(hierarchy, parentHandle.value);
	}

	return (cranh_handle_t){ .value = transformHandle };
}

cranm_transform_t cranh_read_local(cranh_hierarchy_t* hierarchy, cranh_handle_t handle)
{
#ifdef CRANBERRY_DEBUG
	cranh_header_t* header = (cranh_header_t*)hierarchy;
	assert(handle.value < header->currentTransformCount);
#endif // CRANBERRY_DEBUG

	cranh_handle_t parentHandle = *cranh_get_parent(hierarchy, handle.value);
	if (parentHandle.value != cranh_invalid_handle)
	{
		return cranm_inverse_transform(*cranh_get_global(hierarchy, handle.value), *cranh_get_global(hierarchy, parentHandle.value));
	}
	else
	{
		return *cranh_get_global(hierarchy, handle.value);
	}
}

void cranh_write_local(cranh_hierarchy_t* hierarchy, cranh_handle_t handle, cranm_transform_t write)
{
#ifdef CRANBERRY_DEBUG
	cranh_header_t* header = (cranh_header_t*)hierarchy;
	assert(handle.value < header->currentTransformCount);
#endif // CRANBERRY_DEBUG

	*cranh_get_local(hierarchy, handle.value) = write;

	cranh_dirty_scheme_header_t* dirtyScheme = cranh_get_dirty_scheme(hierarchy);
	unsigned int maxChildIndex = *cranh_get_max_child_index(hierarchy, handle.value);
	cranh_dirty_add_interval(dirtyScheme, handle.value, maxChildIndex);
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
#ifdef CRANBERRY_DEBUG
	cranh_header_t* header = (cranh_header_t*)hierarchy;
	assert(handle.value < header->currentTransformCount);
#endif // CRANBERRY_DEBUG

	cranh_handle_t parentHandle = *cranh_get_parent(hierarchy, handle.value);
	if (parentHandle.value != cranh_invalid_handle)
	{
		*cranh_get_local(hierarchy, handle.value) = cranm_inverse_transform(write, *cranh_get_global(hierarchy, parentHandle.value));
	}
	else
	{
		*cranh_get_local(hierarchy, handle.value) = write;
	}

	cranh_dirty_scheme_header_t* dirtyScheme = cranh_get_dirty_scheme(hierarchy);
	unsigned int maxChildIndex = *cranh_get_max_child_index(hierarchy, handle.value);
	cranh_dirty_add_interval(dirtyScheme, handle.value, maxChildIndex);
}

uint8_t cranh_bit_count(uint8_t i)
{
	i = ((i >> 1) & 0x55) + (i & 0x55);
	i = ((i >> 2) & 0x33) + (i & 0x33);
	i = ((i >> 4) & 0x0F) + (i & 0x0F);
	return i;
}

void cranh_transform_locals_to_globals(cranh_hierarchy_t* hierarchy)
{
	cranh_dirty_scheme_header_t* dirtyScheme = cranh_get_dirty_scheme(hierarchy);
	uint8_t* intervalIter = (uint8_t*)(dirtyScheme + 1) + (dirtyScheme->start >> 4);

	cranm_transform_t* localIter = cranh_get_local(hierarchy, dirtyScheme->start);
	cranm_transform_t* globalIter = cranh_get_global(hierarchy, dirtyScheme->start);
	cranh_handle_t* parentIter = cranh_get_parent(hierarchy, dirtyScheme->start);

	unsigned int dirtyStack = 0;
	for (unsigned int in = dirtyScheme->start; in < dirtyScheme->end + 1; 
		in += 4, ++intervalIter, localIter += 4, globalIter += 4, parentIter += 4)
	{
		dirtyStack += cranh_bit_count(*intervalIter & cranh_dirty_start_bit_mask);

		if (dirtyStack > 0)
		{
			for (unsigned int i = 0; i < 4; ++i)
			{
				if ((parentIter + i)->value != cranh_invalid_handle)
				{
#ifdef CRANBERRY_DEBUG
					assert((parentIter + i)->value < in + i);
#endif // CRANBERRY_DEBUG

					cranm_transform_t* parent = cranh_get_global(hierarchy, (parentIter + i)->value);
					*(globalIter + i) = cranm_transform(*(localIter + i), *parent);
				}
				else
				{
					*(globalIter + i) = *(localIter + i);
				}
			}
		}

		dirtyStack -= cranh_bit_count(*intervalIter & cranh_dirty_end_bit_mask);
		*intervalIter = 0;
	}

	dirtyScheme->start = 0;
	dirtyScheme->end = 0;
}


#endif // CRANBERRY_HIERARCHY_IMPL

#endif // __CRANBERRY_HIERARCHY_H

// Thoughts:
// Reparenting a child is currently disallowed. The reason for this is because reparenting adds quite a bit of complexity/performance cost to the
// system. A few options I had considered were keeping the tree in a depth first flattened hierarchy but this immediatly invalidates children indices
// as a result, the fix for that could be to have an additional level of indirection, an array of handles to indices. However, this introduces a data
// dependency to read a transform, you now have to wait for the memory of index array and then the memory for the transform. One option
// could be to allow transforms to be read through raw indices and only ask for conversion when the indices are dirtied. But the complexity of
// maintaining this flat hierarchy, could be quite large. Adding a transform after creating becomes costly, especially with larger buffers.
