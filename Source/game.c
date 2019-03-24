#include "game_cfg.h"
#include "game.h"

#include "cranberry_hierarchy.h"
#include "cranberry_math.h"

#include "3rd/Mist_Profiler.h"

#include <stdint.h>
#include <stdlib.h>

static float randf(float min, float max)
{
	return ((float)rand() / (float)RAND_MAX) * (max - min) + min;
}

#define PI 3.14159f
#define max_entity_count 100000
cranh_hierarchy_t* transform_hierarchy;

const float phys_gravity_a = -9.807f;
const float phys_floor_y = -10.0f;

const float phys_fixed_tick = 0.016f;
static float phys_tick_accumulator = 0.0f;

static float phys_vel_x[max_entity_count];
static float phys_vel_y[max_entity_count];
static float phys_vel_z[max_entity_count];
static float phys_bounce[max_entity_count];

static cranh_handle_t phys_handle[max_entity_count];

static uint32_t phys_entity_count = 0;

static cranh_handle_t render_handles[max_entity_count];
static uint32_t render_count = 0;

void phys_tick(float delta)
{
	phys_tick_accumulator += delta;
	for (; phys_tick_accumulator > phys_fixed_tick; phys_tick_accumulator -= phys_fixed_tick)
	{
		for (uint32_t i = 0; i < phys_entity_count; ++i)
		{
			cranm_transform_t global_transform = cranh_read_global(transform_hierarchy, phys_handle[i]);

			// Apply gravity
			{
				phys_vel_y[i] = phys_vel_y[i] + phys_gravity_a * delta;
			}

			// Apply velocity
			{
				cranm_vec3_t dv = cranm_scale((cranm_vec3_t) { .x = phys_vel_x[i], .y = phys_vel_y[i], .z = phys_vel_z[i] }, delta);
				global_transform.pos = cranm_add3(global_transform.pos, dv);
			}

			// Apply collision
			{
				// We only deal with the floor, once we hit the floor, clamp position, flip velocity
				if (global_transform.pos.y < phys_floor_y)
				{
					phys_vel_y[i] = -phys_vel_y[i] * phys_bounce[i];
					global_transform.pos.y = phys_floor_y;

					cranm_vec3_t randV = { .x = randf(-1.0f, 1.0f),.y = randf(-1.0f, 1.0f),.z = randf(-1.0f, 1.0f) };
					global_transform.rot = cranm_axis_angleq(cranm_normalize3(randV), randf(0.0f, 2.0f * PI));
				}
			}

			cranh_write_global(transform_hierarchy, phys_handle[i], global_transform);
		}
	}
}

void game_init(void)
{
	transform_hierarchy = cranh_create(max_entity_count);

	for (uint32_t i = 0; i < 50000; i++)
	{
		cranm_transform_t t =
		{
			.pos = {randf(-25.0f, 25.0f), randf(0.0f, 15.0f), randf(25.0f, 75.0f)},
			.rot = cranm_axis_angleq((cranm_vec3_t) { .x = 0.0f,.y = 0.0f,.z = 1.0f }, 0.0f),
			.scale = {0.3f, 0.3f, 0.3f}
		};

		cranh_handle_t h = cranh_add(transform_hierarchy, t);
		phys_handle[phys_entity_count] = h;
		phys_vel_x[phys_entity_count] = 0.0f;
		phys_vel_y[phys_entity_count] = 0.0f;
		phys_vel_z[phys_entity_count] = 0.0f;
		phys_bounce[phys_entity_count] = randf(0.8f, 0.9f);
		phys_entity_count++;

		render_handles[render_count] = h;
		render_count++;

		{
			cranm_transform_t c =
			{
				.pos = {3.0f, 5.0f, 0.0f},
				.rot = cranm_axis_angleq((cranm_vec3_t) { .x = 0.0f,.y = 0.0f,.z = 1.0f }, 0.0f),
				.scale = {0.5f, 0.5f, 0.5f}
			};

			cranh_handle_t ch = cranh_add_with_parent(transform_hierarchy, c, h);
			render_handles[render_count] = ch;
			render_count++;

			phys_handle[phys_entity_count] = ch;
			phys_vel_x[phys_entity_count] = 0.0f;
			phys_vel_y[phys_entity_count] = 0.0f;
			phys_vel_z[phys_entity_count] = 0.0f;
			phys_bounce[phys_entity_count] = randf(0.8f, 0.9f);
			phys_entity_count++;
		}
	}
}

void game_tick(float delta)
{
	MIST_PROFILE_BEGIN("game", "game_tick");

	MIST_PROFILE_BEGIN("game", "phys_tick");
	phys_tick(delta);
	MIST_PROFILE_END("game", "phys_tick");

	MIST_PROFILE_BEGIN("game", "transform_tick");
	cranm_transform_locals_to_globals(transform_hierarchy);
	MIST_PROFILE_END("game", "transform_tick");

	MIST_PROFILE_END("game", "game_tick");
}

void game_cleanup(void)
{
	cranh_destroy(transform_hierarchy);
}

unsigned int game_gen_instance_buffer(game_instance_t* buffer, unsigned int maxSize)
{
	for (uint32_t i = 0; i < render_count; i++)
	{
		buffer[i] = (game_instance_t)
		{
			.transform = cranh_read_global(transform_hierarchy, render_handles[i]),
			.color = { 1.0f, 0.7f, 0.0f }
		};
	}

	return render_count;
}