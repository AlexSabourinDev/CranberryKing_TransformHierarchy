#include "game_cfg.h"
#include "game.h"

#include "cranberry_hierarchy.h"
#include "cranberry_math.h"

#include "3rd/Mist_Profiler.h"

#include <stdint.h>
#include <stdlib.h>

#include <Windows.h>
#include <assert.h>

static float randf(float min, float max)
{
	return ((float)rand() / (float)RAND_MAX) * (max - min) + min;
}

#define PI 3.14159f
#define cube_half_dimension 30
#define max_entity_group_count ((cube_half_dimension * 2) * (cube_half_dimension * 2) * (cube_half_dimension * 2) + 10)
#define max_group_count 5
#define max_entity_count (max_entity_group_count * max_group_count)
cranh_hierarchy_t* transform_hierarchy;

const float phys_gravity_a = -9.807f;
const float phys_floor_y = -5.0f;

const float phys_fixed_tick = 0.016f;

static float phys_vel_x[max_group_count][max_entity_group_count];
static float phys_vel_y[max_group_count][max_entity_group_count];
static float phys_vel_z[max_group_count][max_entity_group_count];
static float phys_bounce[max_group_count][max_entity_group_count];

static cranh_handle_t phys_handle[max_group_count][max_entity_group_count];

static uint32_t phys_entity_count[max_group_count] = { 0 };

static cranh_handle_t render_handles[max_entity_count];
static uint32_t render_count = 0;

void phys_tick(unsigned int group);

HANDLE transform_threads[max_group_count];
volatile LONG transform_shutdown = 0;
volatile LONG transform_done_count = 0;
CONDITION_VARIABLE transforms_done;
CONDITION_VARIABLE transform_enabled;
DWORD threadIds[max_group_count];
DWORD WINAPI transform_tick_group(LPVOID lparam)
{
	CRITICAL_SECTION unusedCritical;
	InitializeCriticalSection(&unusedCritical);

	InterlockedIncrement(&transform_done_count);

	unsigned int group = (unsigned int)lparam;
	while (1)
	{
		EnterCriticalSection(&unusedCritical);
		SleepConditionVariableCS(&transform_enabled, &unusedCritical, INFINITE);
		LeaveCriticalSection(&unusedCritical);

		if (transform_shutdown == 1)
		{
			break;
		}

		MIST_PROFILE_BEGIN("game", "phys_thread_tick");
		phys_tick(group);
		MIST_PROFILE_END("game", "phys_thread_tick");

		MIST_PROFILE_BEGIN("game", "transform_thread_tick");
		cranh_transform_locals_to_globals(transform_hierarchy, group);
		MIST_PROFILE_END("game", "transform_thread_tick");

		InterlockedIncrement(&transform_done_count);
		if (transform_done_count == max_group_count)
		{
			WakeConditionVariable(&transforms_done);
		}
	}

	Mist_FlushThreadBuffer();

	return 0;
}

void phys_tick(unsigned int group)
{
	{
		for (uint32_t i = 0; i < phys_entity_count[group]; ++i)
		{
			cranm_transform_t global_transform = cranh_read_global(transform_hierarchy, phys_handle[group][i]);

			// Apply gravity
			{
				phys_vel_y[group][i] = phys_vel_y[group][i] + phys_gravity_a * phys_fixed_tick;
			}

			// Apply velocity
			{
				cranm_vec_t dv = cranm_scale((cranm_vec_t) { .x = phys_vel_x[group][i], .y = phys_vel_y[group][i], .z = phys_vel_z[group][i] }, phys_fixed_tick);
				global_transform.pos = cranm_add3(global_transform.pos, dv);
			}

			// Apply collision
			{
				// We only deal with the floor, once we hit the floor, clamp position, flip velocity
				if (global_transform.pos.y < phys_floor_y)
				{
					phys_vel_y[group][i] = -phys_vel_y[group][i] * phys_bounce[group][i];
					global_transform.pos.y = phys_floor_y;

					cranm_vec_t randV = { .x = randf(-1.0f, 1.0f),.y = randf(-1.0f, 1.0f),.z = randf(-1.0f, 1.0f) };
					global_transform.rot = cranm_axis_angleq(cranm_normalize3(randV), randf(0.0f, 2.0f * PI));
				}
			}

			cranh_write_global(transform_hierarchy, phys_handle[group][i], global_transform);
		}
	}
}

void game_init(void)
{
	transform_hierarchy = cranh_create(max_group_count, max_entity_group_count);

	for (int i = 0; i < max_group_count; i++)
	{
		cranm_vec_t randV = { .x = randf(-1.0f, 1.0f),.y = randf(-1.0f, 1.0f),.z = randf(-1.0f, 1.0f), 0.0f };
		cranm_transform_t t =
		{
			.pos = { (float)((i - 2) * 5), randf(0.0f, 5.0f), randf(15.0f, 25.0f), 0.0f},
			.rot = cranm_axis_angleq(cranm_normalize3(randV), randf(0.0f, 2.0f * PI)),
			.scale = 0.3f
		};

		cranh_handle_t h = cranh_add(transform_hierarchy, t);

		for(int cx = -cube_half_dimension; cx < cube_half_dimension; ++cx)
		{
			for (int cy = -cube_half_dimension; cy < cube_half_dimension; ++cy)
			{
				for (int cz = -cube_half_dimension; cz < cube_half_dimension; ++cz)
				{
					cranm_vec_t crandV = { .x = randf(-1.0f, 1.0f),.y = randf(-1.0f, 1.0f),.z = randf(-1.0f, 1.0f), 0.0f };
					cranm_transform_t c =
					{
						.pos = {cx * 0.75f, cy * 0.75f, cz * 0.75f, 0.0f},
						.rot = cranm_axis_angleq(cranm_normalize3(crandV), randf(0.0f, 2.0f * PI)),
						.scale = 0.1f
					};

					cranh_handle_t ch = cranh_add_with_parent(transform_hierarchy, c, h);
					render_handles[render_count] = ch;
					render_count++;

					phys_handle[i][phys_entity_count[i]] = ch;
					phys_vel_x[i][phys_entity_count[i]] = 0.0f;
					phys_vel_y[i][phys_entity_count[i]] = 0.0f;
					phys_vel_z[i][phys_entity_count[i]] = 0.0f;
					phys_bounce[i][phys_entity_count[i]] = randf(0.95f, 0.99f);
					phys_entity_count[i]++;
				}
			}
		}
	}

	InitializeConditionVariable(&transform_enabled);
	InitializeConditionVariable(&transforms_done);
	for (unsigned int i = 0; i < max_group_count; ++i)
	{
		transform_threads[i] = CreateThread(
			NULL,
			0,
			transform_tick_group,
			(LPVOID)i,
			0,
			&threadIds[i]
		);
	}

	// Wait for all our threads to acknowledge they started
	while (transform_done_count < max_group_count) {};
	InterlockedExchange(&transform_done_count, 0);
}

void game_tick()
{
	MIST_PROFILE_BEGIN("game", "game_tick");

	MIST_PROFILE_BEGIN("game", "thread_tick");

	WakeAllConditionVariable(&transform_enabled);
	
	if (transform_done_count < max_group_count)
	{
		CRITICAL_SECTION unusedCritical;
		InitializeCriticalSection(&unusedCritical);

		EnterCriticalSection(&unusedCritical);
		SleepConditionVariableCS(&transforms_done, &unusedCritical, INFINITE);
		LeaveCriticalSection(&unusedCritical);

		assert(transform_done_count == max_group_count);
		InterlockedExchange(&transform_done_count, 0);
	}

	MIST_PROFILE_END("game", "thread_tick");

	MIST_PROFILE_END("game", "game_tick");
}

void game_cleanup(void)
{
	InterlockedExchange(&transform_shutdown, 1);
	WakeAllConditionVariable(&transform_enabled);
	WaitForMultipleObjects(max_group_count, transform_threads, TRUE, INFINITE);
	for (unsigned int i = 0; i < max_group_count; i++)
	{
		CloseHandle(transform_threads[i]);
	}
	cranh_destroy(transform_hierarchy);
}

unsigned int game_gen_instance_buffer(game_instance_t* buffer, unsigned int maxSize)
{
	MIST_PROFILE_BEGIN("game", "game_gen_instance_buffer");
	assert(render_count <= maxSize);
	for (uint32_t i = 0; i < render_count; i++)
	{
		buffer[i] = (game_instance_t)
		{
			.transform = cranh_read_global(transform_hierarchy, render_handles[i]),
			.color = { 1.0f, 0.7f, 0.0f }
		};
	}
	MIST_PROFILE_END("game", "game_gen_instance_buffer");
	return render_count;
}