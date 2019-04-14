# CranberryKing_TransformHierarchy

This repo attempts to answer the question of the most "effective" approach to implementing a transform hierarchy.

An implementation will be deemed "effective" if it is:
- Fast
- Not overly limiting

Some potential options for the hierarchy are:
- Multithreading?
- Dirty matrices?
- Data oriented design?

The goal is not to have a complete library that someone could implement into their engine, but moreso an answer to what an effective implementation could be. (Although, if you want to try to plug it into your engine, go ahea!)

## Results

https://cranberryking.com/2019/04/14/creating-an-optimized-transform-hierarchy/

This transform hierarchy aims to have a simple API with very little to worry about while still providing good performance.

This library works in fixed step. This means that the transforms are only up to date once cranh_transform_locals_to_globals is called. This decision was made in order to provide a clear view of the transition from the previous frame's position to the current frame's position.

The API is simple:

```C
cranm_transform_t c = { .pos = {.x = 5.0f,.y = 0.0f,.z = 0.0f},.rot = {0},.scale = 1.0f };
cranm_transform_t p = { .pos = {.x = 5.0f,.y = 0.0f,.z = 0.0f},.rot = {0},.scale = 5.0f };

// Group count determines the number of transform groups.
// Transform groups are simply independent groupings of transforms that can be updated
// on various threads without any worries of dependencies.
unsigned int groupCount = 2;
// Group size determines how many transforms can fit in a single group.
unsigned int groupSize = 5;
// cranh_create will allocate the memory for you. If you want to allocate the memory externally,
// call cranh_buffer_size and cranh_buffer_create
cranh_hierarchy_t* hierarchy = cranh_create(groupCount, groupSize);

cranh_handle_t parent = cranh_add(hierarchy, p);
cranh_handle_t child = cranh_add_with_parent(hierarchy, c, parent);

// Instead of a loop, this could be a series of jobs
for(unsigned int i = 0; i < groupCount; ++i)
{
  cranh_transform_locals_to_globals(hierarchy, i);
}

cranh_destroy(hierarchy);
```
