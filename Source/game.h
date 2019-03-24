#pragma once

#include "cranberry_math.h"

typedef struct
{
	cranm_transform_t transform;
	float color[3];
} game_instance_t;

void game_init(void);
void game_tick(float delta);
void game_cleanup(void);

unsigned int game_gen_instance_buffer(game_instance_t* buffer, unsigned int maxSize);
