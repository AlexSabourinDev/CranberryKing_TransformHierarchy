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

unsigned int cranh_group_from_handle(cranh_handle_t handle);

// @brief Create a cranh_hierarchy_t.
// @param groupBufferCount determines the number of transform "groups" the hierarchy supports. Groups are intended to be used as job-able
//        chunks of data.
// @param maxGroupTransformCount Determines the maximum number of transforms this hierarchy can support per group.
// WARNING: This function allocates memory with the standard malloc. It must be released with cranh_destroy.
// If you want to allocate your own memory use the cranh_buffer... family of functions.
cranh_hierarchy_t* cranh_create(unsigned int groupBufferCount, unsigned int maxGroupTransformCount);
// Destroy the cranh_hierarchy created with cranh_create. This will also release the memory allocated by cranh_create.
void cranh_destroy(cranh_hierarchy_t* hierarchy);

// @brief Determines the minimum size necessary to allocate for a buffer.
// @param groupBufferCount determines the number of transform "groups" the hierarchy supports. Groups are intended to be used as job-able
//        chunks of data.
// @param maxGroupTransformCount Determines the maximum number of transforms this hierarchy can support per group.
// Use this function in correspondance with @ref cranh_buffer_create to turn the buffer into a usable chunk of memory.
unsigned int cranh_buffer_size(unsigned int groupBufferCount, unsigned int maxGroupTransformCount);
// @brief Takes a buffer as input and initializes the memory into a workable chunk of memory for the remaining API calls.
// WARNING: cranh_hierarchy_t doesn't have to point to buffer! Call retrieve buffer to get the original pointer.
// @param buffer An externally allocated chunk of memory of a minimum size of at least @ref cranh_buffer_size.
// @param the number of groups to be created for the hierarchy.
// @param the number of transforms that fit in a single group.
cranh_hierarchy_t* cranh_buffer_create(void* buffer, unsigned int groupBufferCount, unsigned int maxGroupTransformCount);


cranh_handle_t cranh_add(cranh_hierarchy_t* hierarchy, cranm_transform_t value);
cranh_handle_t cranh_add_to_group(cranh_hierarchy_t* hierarchy, cranm_transform_t transform, unsigned int group);
cranh_handle_t cranh_add_with_parent(cranh_hierarchy_t* hierarchy, cranm_transform_t value, cranh_handle_t parent);

void cranh_transform_locals_to_globals(cranh_hierarchy_t* hierarchy, unsigned int group);

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
#define cranh_invalid_handle ~0U
#define cranh_buffer_alignment 64
#define cranh_group_bit_count 8
#define cranh_max_group_count ((1 << cranh_group_bit_count) - 1)
#define cranh_transform_bit_count (32 - cranh_group_bit_count)
#define cranh_max_transform_count ((1 << cranh_transform_bit_count) - 1)

typedef struct
{
	unsigned int nextGroup;
	unsigned int groupCount;
	unsigned int maxGroupSize;
} cranh_hierarchy_header_t;

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

unsigned int cranh_group_from_handle(cranh_handle_t handle)
{
	return (handle.value >> cranh_transform_bit_count);
}

unsigned int cranh_index_from_handle(cranh_handle_t handle)
{
	return handle.value & cranh_max_transform_count;
}

cranh_handle_t cranh_create_handle(unsigned int group, unsigned int index)
{
	return (cranh_handle_t) { .value = (group << cranh_transform_bit_count) | index };
}

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
	unsigned int currentChildTransformCount;
	unsigned int currentRootTransformCount; // We keep track of the global transforms so we can easily just memcpy them
} cranh_group_header_t;

// Buffer format:
// header
// local transforms [maxTransformCount]
// global transforms [maxTransformCount]
// parent handles [maxTransformCount]
// max child start + end [maxTransformCount]
// dirty scheme

unsigned int cranh_individual_buffer_size(unsigned int maxGroupTransformCount)
{
	return
		sizeof(cranh_group_header_t) +
		(sizeof(cranm_transform_t) +
			sizeof(cranm_transform_t) +
			sizeof(cranh_handle_t) +
			sizeof(cranh_range_t)) * maxGroupTransformCount +
		cranh_dirty_scheme_size(maxGroupTransformCount) + cranh_buffer_alignment; // Add 64 bytes, we might need that for alignment
}

unsigned int cranh_buffer_size(unsigned int groupBufferCount, unsigned int maxGroupTransformCount)
{
	size_t bufferSize = cranh_individual_buffer_size(maxGroupTransformCount);
	return (unsigned int)(bufferSize * groupBufferCount + sizeof(cranh_hierarchy_header_t));
}

cranh_group_header_t* cranh_retrieve_group_header(cranh_hierarchy_t* hierarchy, unsigned int group)
{
	cranh_hierarchy_header_t* header = (cranh_hierarchy_header_t*)hierarchy;

	intptr_t bufferAddress = (intptr_t)hierarchy;
	bufferAddress += sizeof(cranh_hierarchy_header_t) + cranh_individual_buffer_size(header->maxGroupSize) * group;
	intptr_t offset = cranh_buffer_alignment - (bufferAddress + sizeof(cranh_group_header_t)) % cranh_buffer_alignment;
	return (cranh_group_header_t*)(bufferAddress + offset);
}

cranh_hierarchy_t* cranh_create(unsigned int groupCount, unsigned int maxGroupTransformCount)
{
	unsigned int bufferSize = cranh_buffer_size(groupCount, maxGroupTransformCount);
	void* buffer = malloc(bufferSize);
	return cranh_buffer_create(buffer, groupCount, maxGroupTransformCount);
}

void cranh_destroy(cranh_hierarchy_t* hierarchy)
{
	free(hierarchy);
}

cranh_dirty_scheme_header_t* cranh_get_dirty_scheme(cranh_hierarchy_t* hierarchy, cranh_group_header_t* group);
void cranh_group_create(cranh_hierarchy_t* hierarchy, void* groupBuffer)
{
	// We add the size of the header because we don't want to align the header.
	// we want to align the local buffer and global buffer
	intptr_t bufferAddress = (intptr_t)groupBuffer;
	intptr_t offset = cranh_buffer_alignment - (bufferAddress + sizeof(cranh_group_header_t)) % cranh_buffer_alignment;
	groupBuffer = (void*)(bufferAddress + offset);
#ifdef CRANBERRY_DEBUG
	assert(offset <= UINT8_MAX);
	assert(((intptr_t)groupBuffer + sizeof(cranh_group_header_t)) % cranh_buffer_alignment == 0);
#endif // CRANBERRY_DEBUG

	cranh_group_header_t* groupHeader = (cranh_group_header_t*)groupBuffer;
	groupHeader->currentChildTransformCount = 0;
	groupHeader->currentRootTransformCount = 0;
	cranh_dirty_reset(cranh_get_dirty_scheme(hierarchy, groupHeader));
}

cranh_hierarchy_t* cranh_buffer_create(void* buffer, unsigned int groupCount, unsigned int maxGroupSize)
{
#ifdef CRANBERRY_DEBUG
	assert(groupCount < cranh_max_group_count);
	assert(maxGroupSize < cranh_max_transform_count);
#endif // CRANBERRY_DEBUG

	unsigned int bufferSize = cranh_buffer_size(groupCount, maxGroupSize);
	unsigned int groupSize = cranh_individual_buffer_size(maxGroupSize);

	// Zero out our buffer before we work with it
	memset(buffer, 0, bufferSize);

	cranh_hierarchy_header_t* hierarchyHeader = (cranh_hierarchy_header_t*)buffer;
	hierarchyHeader->nextGroup = 0;
	hierarchyHeader->groupCount = groupCount;
	hierarchyHeader->maxGroupSize = maxGroupSize;

	for (unsigned int i = 0; i < groupCount; ++i)
	{
		void* groupBuffer = ((uint8_t*)buffer) + groupSize * i + sizeof(cranh_hierarchy_header_t);
		cranh_group_create((cranh_hierarchy_t*)hierarchyHeader, groupBuffer);
	}

	return (cranh_hierarchy_t*)hierarchyHeader;
}

// Globals are the first buffer
cranm_transform_t* cranh_get_global(cranh_hierarchy_t* hierarchy, cranh_group_header_t* group, unsigned int index)
{
	// hierarchy unused in this one, but keeping it in the api for consistency
	(void)hierarchy;

	uint8_t* bufferStart = (uint8_t*)group;
	bufferStart += sizeof(cranh_group_header_t);
	return (cranm_transform_t*)bufferStart + index;
}

// Locals are the second buffer.
cranm_transform_t* cranh_get_local(cranh_hierarchy_t* hierarchy, cranh_group_header_t* group, unsigned int index)
{
	unsigned int maxGroupSize = ((cranh_hierarchy_header_t*)hierarchy)->maxGroupSize;

	uint8_t* bufferStart = (uint8_t*)group;
	bufferStart += sizeof(cranh_group_header_t) + sizeof(cranm_transform_t) * maxGroupSize;
	return (cranm_transform_t*)bufferStart + index;
}

// Indices are the third buffer
cranh_handle_t* cranh_get_parent(cranh_hierarchy_t* hierarchy, cranh_group_header_t* group, unsigned int index)
{
	unsigned int maxGroupSize = ((cranh_hierarchy_header_t*)hierarchy)->maxGroupSize;

	uint8_t* bufferStart = (uint8_t*)group;
	bufferStart += sizeof(cranh_group_header_t) + sizeof(cranm_transform_t) * 2 * maxGroupSize;
	return (cranh_handle_t*)bufferStart + index;
}

// Max child index is the fourth buffer
cranh_range_t* cranh_get_children_range(cranh_hierarchy_t* hierarchy, cranh_group_header_t* group, unsigned int index)
{
	unsigned int maxGroupSize = ((cranh_hierarchy_header_t*)hierarchy)->maxGroupSize;

	uint8_t* bufferStart = (uint8_t*)group;
	bufferStart += sizeof(cranh_group_header_t) + (sizeof(cranm_transform_t) * 2 + sizeof(cranh_handle_t)) * maxGroupSize;
	return (cranh_range_t*)bufferStart + index;
}

// Dirty scheme is the fifth buffer
cranh_dirty_scheme_header_t* cranh_get_dirty_scheme(cranh_hierarchy_t* hierarchy, cranh_group_header_t* group)
{
	unsigned int maxGroupSize = ((cranh_hierarchy_header_t*)hierarchy)->maxGroupSize;

	uint8_t* bufferStart = (uint8_t*)group;
	bufferStart += sizeof(cranh_group_header_t) + (sizeof(cranm_transform_t) * 2 + sizeof(cranh_handle_t) + sizeof(cranh_range_t)) * maxGroupSize;
	return (cranh_dirty_scheme_header_t*)bufferStart;
}

cranh_handle_t cranh_add(cranh_hierarchy_t* hierarchy, cranm_transform_t transform)
{
	cranh_hierarchy_header_t* hierarchyHeader = (cranh_hierarchy_header_t*)hierarchy;

	unsigned int group = hierarchyHeader->nextGroup;
	hierarchyHeader->nextGroup = (hierarchyHeader->nextGroup + 1) % hierarchyHeader->groupCount;
	return cranh_add_to_group(hierarchy, transform, group);
}

cranh_handle_t cranh_add_to_group(cranh_hierarchy_t* hierarchy, cranm_transform_t transform, unsigned int group)
{
	unsigned int maxGroupSize = ((cranh_hierarchy_header_t*)hierarchy)->maxGroupSize;

	cranh_group_header_t* header = cranh_retrieve_group_header(hierarchy, group);
	++header->currentRootTransformCount;
	unsigned int transformHandle = maxGroupSize - header->currentRootTransformCount;

#ifdef CRANBERRY_DEBUG
	// If our transform handle is less than our child transform count, that means our memory has overflwed.
	assert(transformHandle > header->currentChildTransformCount);
#endif // CRANBERRY_DEBUG

	cranm_transform_t* local = cranh_get_local(hierarchy, header, transformHandle);
	cranm_transform_t* global = cranh_get_global(hierarchy, header, transformHandle);
	cranh_handle_t* parent = cranh_get_parent(hierarchy, header, transformHandle);

	parent->value = cranh_invalid_handle;
	*global = transform;
	*local = transform;

	// dirty setup
	cranh_range_t* currentChildrenRange = cranh_get_children_range(hierarchy, header, transformHandle);
	currentChildrenRange->start = cranh_invalid_handle;
	currentChildrenRange->end = 0;

	return cranh_create_handle(group, transformHandle);
}

cranh_handle_t cranh_add_with_parent(cranh_hierarchy_t* hierarchy, cranm_transform_t transform, cranh_handle_t parentHandle)
{
	unsigned int parentGroup = cranh_group_from_handle(parentHandle);
	unsigned int parentIndex = cranh_index_from_handle(parentHandle);

	cranh_group_header_t* header = cranh_retrieve_group_header(hierarchy, parentGroup);
	unsigned int transformHandle = header->currentChildTransformCount;
	++header->currentChildTransformCount;

#ifdef CRANBERRY_DEBUG
	unsigned int maxGroupSize = ((cranh_hierarchy_header_t*)hierarchy)->maxGroupSize;

	assert(transformHandle < maxGroupSize);
	assert(
		   (parentIndex < header->currentChildTransformCount && parentIndex < transformHandle)
		|| maxGroupSize - parentIndex <= header->currentRootTransformCount);
#endif // CRANBERRY_DEBUG

	cranm_transform_t* local = cranh_get_local(hierarchy, header, transformHandle);
	cranm_transform_t* global = cranh_get_global(hierarchy, header, transformHandle);
	cranh_handle_t* parent = cranh_get_parent(hierarchy, header, transformHandle);

	*parent = parentHandle;
	*global = cranm_transform(transform, *cranh_get_global(hierarchy, header, parentIndex));
	*local = transform;

	cranh_range_t* currentChildrenRange = cranh_get_children_range(hierarchy, header, transformHandle);
	currentChildrenRange->start = cranh_invalid_handle;
	currentChildrenRange->end = 0;

	// Update all of the parents
	while (parentHandle.value != cranh_invalid_handle)
	{
		// We don't actually need the parent group, all children should be in the same group
		// unsigned int searchParentGroup = cranh_group_from_handle(parentHandle);
		unsigned int searchParentIndex = cranh_index_from_handle(parentHandle);

		cranh_range_t* parentChildrenRange = cranh_get_children_range(hierarchy, header, searchParentIndex);

		parentChildrenRange->start = parentChildrenRange->start == cranh_invalid_handle ? transformHandle : parentChildrenRange->start;
#ifdef CRANBERRY_DEBUG
		assert(parentChildrenRange->start <= transformHandle);
#endif // CRANBERRY_DEBUG
		parentChildrenRange->end = parentChildrenRange->end < transformHandle ? transformHandle : parentChildrenRange->end;

		parentHandle = *cranh_get_parent(hierarchy, header, searchParentIndex);
	}

	return cranh_create_handle(parentGroup, transformHandle);
}

cranm_transform_t cranh_read_local(cranh_hierarchy_t* hierarchy, cranh_handle_t handle)
{
	unsigned int group = cranh_group_from_handle(handle);
	unsigned int index = cranh_index_from_handle(handle);

	cranh_group_header_t* header = cranh_retrieve_group_header(hierarchy, group);
#ifdef CRANBERRY_DEBUG
	unsigned int maxGroupSize = ((cranh_hierarchy_header_t*)hierarchy)->maxGroupSize;
	assert(index < header->currentChildTransformCount || maxGroupSize - index < header->currentRootTransformCount);
#endif // CRANBERRY_DEBUG

	cranh_handle_t parentHandle = *cranh_get_parent(hierarchy, header, index);
	if (parentHandle.value != cranh_invalid_handle)
	{
		// We don't need the parent group, it should be in the same group as the child
		// unsigned int parentGroup = cranh_group_from_handle(parentHandle);
		unsigned int parentIndex = cranh_index_from_handle(parentHandle);
		return cranm_inverse_transform(*cranh_get_global(hierarchy, header, index), *cranh_get_global(hierarchy, header, parentIndex));
	}
	else
	{
		return *cranh_get_global(hierarchy, header, index);
	}
}

void cranh_write_local(cranh_hierarchy_t* hierarchy, cranh_handle_t handle, cranm_transform_t write)
{
	unsigned int group = cranh_group_from_handle(handle);
	unsigned int index = cranh_index_from_handle(handle);

	cranh_group_header_t* header = cranh_retrieve_group_header(hierarchy, group);

#ifdef CRANBERRY_DEBUG
	unsigned int maxGroupSize = ((cranh_hierarchy_header_t*)hierarchy)->maxGroupSize;
	assert(index < header->currentChildTransformCount || maxGroupSize - index < header->currentRootTransformCount);
#endif // CRANBERRY_DEBUG

	*cranh_get_local(hierarchy, header, index) = write;

	cranh_dirty_scheme_header_t* dirtyScheme = cranh_get_dirty_scheme(hierarchy, header);
	cranh_range_t* childrenRange = cranh_get_children_range(hierarchy, header, index);

	// If we're a transform at the end of the buffer, we're a root
	if (index > header->currentChildTransformCount)
	{
		cranh_dirty_add_root(dirtyScheme, index);
	}
	else
	{
		cranh_dirty_add_child(dirtyScheme, index);
	}

	if (childrenRange->start != cranh_invalid_handle)
	{
		cranh_dirty_add_child_interval(dirtyScheme, *childrenRange);
	}
}

cranm_transform_t cranh_read_global(cranh_hierarchy_t* hierarchy, cranh_handle_t handle)
{
	unsigned int group = cranh_group_from_handle(handle);
	unsigned int index = cranh_index_from_handle(handle);

	cranh_group_header_t* header = cranh_retrieve_group_header(hierarchy, group);
#ifdef CRANBERRY_DEBUG
	unsigned int maxGroupSize = ((cranh_hierarchy_header_t*)hierarchy)->maxGroupSize;
	assert(index < header->currentChildTransformCount || maxGroupSize - index <= header->currentRootTransformCount);
#endif // CRANBERRY_DEBUG

	return *cranh_get_global(hierarchy, header, index);
}

void cranh_write_global(cranh_hierarchy_t* hierarchy, cranh_handle_t handle, cranm_transform_t write)
{
	unsigned int group = cranh_group_from_handle(handle);
	unsigned int index = cranh_index_from_handle(handle);

	cranh_group_header_t* header = cranh_retrieve_group_header(hierarchy, group);
#ifdef CRANBERRY_DEBUG
	unsigned int maxGroupSize = ((cranh_hierarchy_header_t*)hierarchy)->maxGroupSize;
	assert(index < header->currentChildTransformCount || maxGroupSize - index <= header->currentRootTransformCount);
#endif // CRANBERRY_DEBUG

	cranh_dirty_scheme_header_t* dirtyScheme = cranh_get_dirty_scheme(hierarchy, header);

	cranh_handle_t parentHandle = *cranh_get_parent(hierarchy, header, index);
	if (parentHandle.value != cranh_invalid_handle)
	{
		// Parent should be in the same group as the child
		// unsigned int parentGroup = cranh_group_from_handle(parentHandle);
		unsigned int parentIndex = cranh_index_from_handle(parentHandle);

		*cranh_get_local(hierarchy, header, index) = cranm_inverse_transform(write, *cranh_get_global(hierarchy, header, parentIndex));
		cranh_dirty_add_child(dirtyScheme, index);
	}
	else
	{
		*cranh_get_local(hierarchy, header, index) = write;
		cranh_dirty_add_root(dirtyScheme, index);
	}

	cranh_range_t* childrenRange = cranh_get_children_range(hierarchy, header, index);
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

void cranh_transform_locals_to_globals(cranh_hierarchy_t* hierarchy, unsigned int group)
{
	cranh_group_header_t* header = cranh_retrieve_group_header(hierarchy, group);
	cranh_dirty_scheme_header_t* dirtyScheme = cranh_get_dirty_scheme(hierarchy, header);

	// Transform root transforms
	{
		uint8_t* rootStart = cranh_dirty_read(dirtyScheme, dirtyScheme->rootStart);
		uint8_t* rootEnd = cranh_dirty_read(dirtyScheme, dirtyScheme->rootEnd);

		cranm_transform_t* localIter = cranh_get_local(hierarchy, header, dirtyScheme->rootStart);
		cranm_transform_t* globalIter = cranh_get_global(hierarchy, header, dirtyScheme->rootStart);

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

		cranm_transform_t* localIter = cranh_get_local(hierarchy, header, dirtyScheme->childStart);
		cranm_transform_t* globalIter = cranh_get_global(hierarchy, header, dirtyScheme->childStart);
		cranh_handle_t* parentIter = cranh_get_parent(hierarchy, header, dirtyScheme->childStart);

		unsigned int dirtyStack = 0;
		for (uint8_t* iter = childStart; iter <= childEnd; ++iter, localIter += 4, globalIter += 4, parentIter += 4)
		{
			dirtyStack += cranh_bit_count(*iter & cranh_dirty_start_bit_mask);
			if (dirtyStack > 0)
			{
				for (unsigned int i = 0; i < 4; ++i)
				{
					unsigned int parentIndex = cranh_index_from_handle(*(parentIter + i));

#ifdef CRANBERRY_DEBUG
					unsigned int maxGroupSize = ((cranh_hierarchy_header_t*)hierarchy)->maxGroupSize;

					intptr_t currentTransformIndex = localIter - cranh_get_local(hierarchy, header, dirtyScheme->childStart);
					assert(
						(parentIndex < header->currentChildTransformCount && parentIndex < currentTransformIndex)
						|| maxGroupSize - parentIndex <= header->currentRootTransformCount);
#endif // CRANBERRY_DEBUG

					cranm_transform_t* parent = cranh_get_global(hierarchy, header, parentIndex);
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
