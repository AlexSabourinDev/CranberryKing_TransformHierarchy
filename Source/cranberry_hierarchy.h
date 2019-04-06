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
// WARNING: cranh_hierarchy_t doesn't have to point to buffer! Call retrieve buffer to get the original pointer.
// @param buffer An externally allocated chunk of memory of a minimum size of at least @ref cranh_buffer_size.
cranh_hierarchy_t* cranh_buffer_create(void* buffer, unsigned int maxTransformCount);

void* cranh_retrieve_buffer(cranh_hierarchy_t* hierarchy);


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
#define cranh_buffer_alignment 64

typedef struct
{
	unsigned int start;
	unsigned int end;
} cranh_range_t;

typedef struct
{
	unsigned int childStart;
	unsigned int childEnd;
	unsigned int rootStart;
	unsigned int rootEnd;
} cranh_dirty_scheme_header_t;

unsigned int cranh_dirty_scheme_size(unsigned int maxTransformCount)
{
	return sizeof(cranh_dirty_scheme_header_t) + sizeof(uint32_t) * (maxTransformCount / 16 + 1);
}

void cranh_dirty_reset(cranh_dirty_scheme_header_t* header)
{
	header->rootStart = cranh_invalid_handle;
	header->rootEnd = 0;
	header->childStart = cranh_invalid_handle;
	header->childEnd = 0;
}

void cranh_dirty_add_root(cranh_dirty_scheme_header_t* intervalSetHeader, unsigned int index)
{
	uint32_t* dirtyStream = (uint32_t*)(intervalSetHeader + 1);
	uint32_t* dirty = dirtyStream + (index >> 4);
	*dirty = *dirty | ((cranh_dirty_start_flag | cranh_dirty_end_flag) << ((index & 0x0F) << 1));

	intervalSetHeader->rootStart = index < intervalSetHeader->rootStart ? index & ~0x03 : intervalSetHeader->rootStart;
	intervalSetHeader->rootEnd = index > intervalSetHeader->rootEnd ? index & ~0x03 : intervalSetHeader->rootEnd;
}

void cranh_dirty_add_child(cranh_dirty_scheme_header_t* intervalSetHeader, unsigned int index)
{
	uint32_t* dirtyStream = (uint32_t*)(intervalSetHeader + 1);
	uint32_t* dirty = dirtyStream + (index >> 4);
	*dirty = *dirty | ((cranh_dirty_start_flag | cranh_dirty_end_flag) << ((index & 0x0F) << 1));

	intervalSetHeader->childStart = index < intervalSetHeader->childStart ? index & ~0x03 : intervalSetHeader->childStart;
	intervalSetHeader->childEnd = index > intervalSetHeader->childEnd ? index & ~0x03 : intervalSetHeader->childEnd;
}

void cranh_dirty_add_child_interval(cranh_dirty_scheme_header_t* intervalSetHeader, cranh_range_t range)
{
#ifdef CRANBERRY_DEBUG
	assert(range.start <= range.end);
#endif // CRANBERRY_DEBUG

	uint32_t* dirtyStream = (uint32_t*)(intervalSetHeader + 1);
	uint32_t* dirtyStart = dirtyStream + (range.start >> 4);
	*dirtyStart = *dirtyStart | (cranh_dirty_start_flag << ((range.start & 0x0F) << 1));

	uint32_t* dirtyEnd = dirtyStream + (range.end >> 4);
	*dirtyEnd = *dirtyEnd | (cranh_dirty_end_flag << ((range.end & 0x0F) << 1));

	intervalSetHeader->childStart = range.start < intervalSetHeader->childStart ? range.start & ~0x03 : intervalSetHeader->childStart;
	intervalSetHeader->childEnd = range.end > intervalSetHeader->childEnd ? range.end & ~0x03 : intervalSetHeader->childEnd;
}

uint8_t* cranh_dirty_read(cranh_dirty_scheme_header_t* header, unsigned int index)
{
	return ((uint8_t*)(header + 1)) + (index >> 2);
}

typedef struct
{
	unsigned int maxTransformCount;
	unsigned int currentChildTransformCount;
	unsigned int currentRootTransformCount; // We keep track of the global transforms so we can easily just memcpy them
} cranh_header_t;

// Buffer format:
// header
// local transforms [maxTransformCount]
// global transforms [maxTransformCount]
// parent handles [maxTransformCount]
// max child start + end [maxTransformCount]
// dirty scheme

unsigned int cranh_buffer_size(unsigned int maxTransformCount)
{
	return 
		sizeof(cranh_header_t) +
		(sizeof(cranm_transform_t) +
		sizeof(cranm_transform_t) +
		sizeof(cranh_handle_t) + 
		sizeof(cranh_range_t)) * maxTransformCount +
		cranh_dirty_scheme_size(maxTransformCount) + cranh_buffer_alignment; // Add 64 bytes, we might need that for alignment
}

void* cranh_retrieve_buffer(cranh_hierarchy_t* hierarchy)
{
	uint8_t offset = *((uint8_t*)hierarchy - 1);
	void* buffer = (void*)((intptr_t)hierarchy - offset);
	return buffer;
}

cranh_hierarchy_t* cranh_create(unsigned int maxTransformCount)
{
	unsigned int bufferSize = cranh_buffer_size(maxTransformCount);
	void* buffer = malloc(bufferSize);
	return cranh_buffer_create(buffer, maxTransformCount);
}

void cranh_destroy(cranh_hierarchy_t* hierarchy)
{
	void* buffer = cranh_retrieve_buffer(hierarchy);
	free(buffer);
}

cranh_dirty_scheme_header_t* cranh_get_dirty_scheme(cranh_hierarchy_t* hierarchy);
cranh_hierarchy_t* cranh_buffer_create(void* buffer, unsigned int maxSize)
{
	// Zero out our buffer before we work with it
	memset(buffer, 0, cranh_buffer_size(maxSize));

	// We add the size of the header because we don't want to align the header.
	// we want to align the local buffer and global buffer
	intptr_t bufferAddress = (intptr_t)buffer;
	intptr_t offset = cranh_buffer_alignment - (bufferAddress + sizeof(cranh_header_t)) % cranh_buffer_alignment;
	buffer = (void*)(bufferAddress + offset);
#ifdef CRANBERRY_DEBUG
	assert(offset <= UINT8_MAX);
	assert(((intptr_t)buffer + sizeof(cranh_header_t)) % cranh_buffer_alignment == 0);
#endif // CRANBERRY_DEBUG
	*((uint8_t*)buffer - 1) = (uint8_t)offset;

	cranh_header_t* hierarchyHeader = (cranh_header_t*)buffer;
	hierarchyHeader->maxTransformCount = maxSize;
	hierarchyHeader->currentChildTransformCount = 0;
	hierarchyHeader->currentRootTransformCount = 0;
	cranh_dirty_reset(cranh_get_dirty_scheme((cranh_hierarchy_t*)hierarchyHeader));
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
cranh_range_t* cranh_get_children_range(cranh_hierarchy_t* hierarchy, unsigned int index)
{
	cranh_header_t* header = (cranh_header_t*)hierarchy;

	uint8_t* bufferStart = (uint8_t*)hierarchy;
	bufferStart += sizeof(cranh_header_t) + (sizeof(cranm_transform_t) * 2 + sizeof(cranh_handle_t)) * header->maxTransformCount;
	return (cranh_range_t*)bufferStart + index;
}

// Dirty scheme is the fifth buffer
cranh_dirty_scheme_header_t* cranh_get_dirty_scheme(cranh_hierarchy_t* hierarchy)
{
	cranh_header_t* header = (cranh_header_t*)hierarchy;

	uint8_t* bufferStart = (uint8_t*)hierarchy;
	bufferStart += sizeof(cranh_header_t) + (sizeof(cranm_transform_t) * 2 + sizeof(cranh_handle_t) + sizeof(cranh_range_t)) * header->maxTransformCount;
	return (cranh_dirty_scheme_header_t*)bufferStart;
}

cranh_handle_t cranh_add(cranh_hierarchy_t* hierarchy, cranm_transform_t value)
{
	cranh_header_t* header = (cranh_header_t*)hierarchy;
	++header->currentRootTransformCount;
	unsigned int transformHandle = header->maxTransformCount - header->currentRootTransformCount;

#ifdef CRANBERRY_DEBUG
	// If our transform handle is less than our child transform count, that means our memory has overflwed.
	assert(transformHandle > header->currentChildTransformCount);
#endif // CRANBERRY_DEBUG

	cranm_transform_t* local = cranh_get_local(hierarchy, transformHandle);
	cranm_transform_t* global = cranh_get_global(hierarchy, transformHandle);
	cranh_handle_t* parent = cranh_get_parent(hierarchy, transformHandle);

	parent->value = cranh_invalid_handle;
	*global = value;
	*local = value;

	// dirty setup
	cranh_range_t* currentChildrenRange = cranh_get_children_range(hierarchy, transformHandle);
	currentChildrenRange->start = cranh_invalid_handle;
	currentChildrenRange->end = 0;

	return (cranh_handle_t){ .value = transformHandle };
}

cranh_handle_t cranh_add_with_parent(cranh_hierarchy_t* hierarchy, cranm_transform_t value, cranh_handle_t parentHandle)
{
	cranh_header_t* header = (cranh_header_t*)hierarchy;
	unsigned int transformHandle = header->currentChildTransformCount;
	++header->currentChildTransformCount;

#ifdef CRANBERRY_DEBUG
	assert(transformHandle < header->maxTransformCount);
	assert(
		   (parentHandle.value < header->currentChildTransformCount && parentHandle.value < transformHandle)
		|| header->maxTransformCount - parentHandle.value <= header->currentRootTransformCount);
#endif // CRANBERRY_DEBUG

	cranm_transform_t* local = cranh_get_local(hierarchy, transformHandle);
	cranm_transform_t* global = cranh_get_global(hierarchy, transformHandle);
	cranh_handle_t* parent = cranh_get_parent(hierarchy, transformHandle);

	*parent = parentHandle;
	*global = cranm_transform(value, *cranh_get_global(hierarchy, parentHandle.value));
	*local = value;

	cranh_range_t* currentChildrenRange = cranh_get_children_range(hierarchy, transformHandle);
	currentChildrenRange->start = cranh_invalid_handle;
	currentChildrenRange->end = 0;

	// Update all of the parents
	while (parentHandle.value != cranh_invalid_handle)
	{
		cranh_range_t* parentChildrenRange = cranh_get_children_range(hierarchy, parentHandle.value);

		parentChildrenRange->start = parentChildrenRange->start == cranh_invalid_handle ? transformHandle : parentChildrenRange->start;
#ifdef CRANBERRY_DEBUG
		assert(parentChildrenRange->start <= transformHandle);
#endif // CRANBERRY_DEBUG
		parentChildrenRange->end = parentChildrenRange->end < transformHandle ? transformHandle : parentChildrenRange->end;

		parentHandle = *cranh_get_parent(hierarchy, parentHandle.value);
	}

	return (cranh_handle_t){ .value = transformHandle };
}

cranm_transform_t cranh_read_local(cranh_hierarchy_t* hierarchy, cranh_handle_t handle)
{
#ifdef CRANBERRY_DEBUG
	cranh_header_t* header = (cranh_header_t*)hierarchy;
	assert(handle.value < header->currentChildTransformCount || header->maxTransformCount - handle.value < header->currentRootTransformCount);
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
	cranh_header_t* header = (cranh_header_t*)hierarchy;

#ifdef CRANBERRY_DEBUG
	assert(handle.value < header->currentChildTransformCount || header->maxTransformCount - handle.value < header->currentRootTransformCount);
#endif // CRANBERRY_DEBUG

	*cranh_get_local(hierarchy, handle.value) = write;

	cranh_dirty_scheme_header_t* dirtyScheme = cranh_get_dirty_scheme(hierarchy);
	cranh_range_t* childrenRange = cranh_get_children_range(hierarchy, handle.value);

	if (handle.value > header->currentChildTransformCount)
	{
		cranh_dirty_add_root(dirtyScheme, handle.value);
	}
	else
	{
		cranh_dirty_add_child(dirtyScheme, handle.value);
	}

	if (childrenRange->start != cranh_invalid_handle)
	{
		cranh_dirty_add_child_interval(dirtyScheme, *childrenRange);
	}
}

cranm_transform_t cranh_read_global(cranh_hierarchy_t* hierarchy, cranh_handle_t handle)
{
#ifdef CRANBERRY_DEBUG
	cranh_header_t* header = (cranh_header_t*)hierarchy;
	assert(handle.value < header->currentChildTransformCount || header->maxTransformCount - handle.value <= header->currentRootTransformCount);
#endif // CRANBERRY_DEBUG

	return *cranh_get_global(hierarchy, handle.value);
}

void cranh_write_global(cranh_hierarchy_t* hierarchy, cranh_handle_t handle, cranm_transform_t write)
{
#ifdef CRANBERRY_DEBUG
	cranh_header_t* header = (cranh_header_t*)hierarchy;
	assert(handle.value < header->currentChildTransformCount || header->maxTransformCount - handle.value <= header->currentRootTransformCount);
#endif // CRANBERRY_DEBUG

	cranh_dirty_scheme_header_t* dirtyScheme = cranh_get_dirty_scheme(hierarchy);

	cranh_handle_t parentHandle = *cranh_get_parent(hierarchy, handle.value);
	if (parentHandle.value != cranh_invalid_handle)
	{
		*cranh_get_local(hierarchy, handle.value) = cranm_inverse_transform(write, *cranh_get_global(hierarchy, parentHandle.value));
		cranh_dirty_add_child(dirtyScheme, handle.value);
	}
	else
	{
		*cranh_get_local(hierarchy, handle.value) = write;
		cranh_dirty_add_root(dirtyScheme, handle.value);
	}

	cranh_range_t* childrenRange = cranh_get_children_range(hierarchy, handle.value);
	if (childrenRange->start != cranh_invalid_handle)
	{
		cranh_dirty_add_child_interval(dirtyScheme, *childrenRange);
	}
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

	// Transform root transforms
	{
		uint8_t* rootStart = cranh_dirty_read(dirtyScheme, dirtyScheme->rootStart);
		uint8_t* rootEnd = cranh_dirty_read(dirtyScheme, dirtyScheme->rootEnd);

		cranm_transform_t* localIter = cranh_get_local(hierarchy, dirtyScheme->rootStart);
		cranm_transform_t* globalIter = cranh_get_global(hierarchy, dirtyScheme->rootStart);

		unsigned int dirtyStack = 0;
		for (uint8_t* iter = rootStart; iter <= rootEnd; ++iter, localIter += 4, globalIter += 4)
		{
			dirtyStack += cranh_bit_count(*iter & cranh_dirty_start_bit_mask);
			if (dirtyStack > 0)
			{
				memcpy(globalIter, localIter, sizeof(cranm_transform_t) * 4);
			}
			dirtyStack -= cranh_bit_count(*iter & cranh_dirty_end_bit_mask);
		}
	}

	// Children transforms
	{
		uint8_t* childStart = cranh_dirty_read(dirtyScheme, dirtyScheme->childStart);
		uint8_t* childEnd = cranh_dirty_read(dirtyScheme, dirtyScheme->childEnd);

		cranm_transform_t* localIter = cranh_get_local(hierarchy, dirtyScheme->childStart);
		cranm_transform_t* globalIter = cranh_get_global(hierarchy, dirtyScheme->childStart);
		cranh_handle_t* parentIter = cranh_get_parent(hierarchy, dirtyScheme->childStart);

		unsigned int dirtyStack = 0;
		for (uint8_t* iter = childStart; iter <= childEnd; ++iter, localIter += 4, globalIter += 4, parentIter += 4)
		{
			dirtyStack += cranh_bit_count(*iter & cranh_dirty_start_bit_mask);
			if (dirtyStack > 0)
			{
				for (unsigned int i = 0; i < 4; ++i)
				{
					unsigned int parentIndex = (parentIter + i)->value;

#ifdef CRANBERRY_DEBUG
					intptr_t currentTransformIndex = localIter - cranh_get_local(hierarchy, dirtyScheme->childStart);
					cranh_header_t* header = (cranh_header_t*)hierarchy;
					assert(
						(parentIndex < header->currentChildTransformCount && parentIndex < currentTransformIndex)
						|| header->maxTransformCount - parentIndex <= header->currentRootTransformCount);
#endif // CRANBERRY_DEBUG

					cranm_transform_t* parent = cranh_get_global(hierarchy, parentIndex);
					*(globalIter + i) = cranm_transform(*(localIter + i), *parent);
				}
			}
			dirtyStack -= cranh_bit_count(*iter & cranh_dirty_end_bit_mask);
		}
	}

	cranh_dirty_reset(dirtyScheme);
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
