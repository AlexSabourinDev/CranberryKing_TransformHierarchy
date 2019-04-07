#include "game_cfg.h"
#include "game_shaders.h"
#include "game.h"

#define CRANBERRY_HIERARCHY_IMPL
#include "cranberry_hierarchy.h"
#include "cranberry_math.h"

#include <stdio.h>

#define SOKOL_IMPL
#define SOKOL_GLCORE33
#define SOKOL_DEBUG
#define SOKOL_WIN32_FORCE_MAIN
#include "3rd/sokol_app.h"
#include "3rd/sokol_gfx.h"
#include "3rd/sokol_time.h"

#define MIST_PROFILE_IMPLEMENTATION
#include "3rd/Mist_Profiler.h"

#ifdef CRANBERRY_ENABLE_TESTS
#include <assert.h>
#include <string.h>

void test()
{
	cranm_transform_t c = { .pos = {.x = 5.0f,.y = 0.0f,.z = 0.0f},.rot = {0},.scale = {.x = 1.0f,.y = 1.0f,.z = 1.0f} };

	cranm_transform_t p = { .pos = {.x = 5.0f,.y = 0.0f,.z = 0.0f},.rot = {0},.scale = {.x = 5.0f,.y = 5.0f,.z = 5.0f} };

	cranm_transform_t rt = cranm_transform(&c, &p);

	cranm_transform_t t = { .pos = {.x = 30.0f,.y = 0.0f,.z = 0.0f},.rot = {0},.scale = {5.0f, 5.0f, 5.0f} };
	assert(memcmp(&rt, &t, sizeof(cranm_transform_t)) == 0);

	cranh_hierarchy_t* hierarchy = cranh_create(5);
	cranh_handle_t parent = cranh_add(hierarchy, p);
	cranh_handle_t child = cranh_add_with_parent(hierarchy, c, parent);

	cranm_transform_t childGlobal = cranh_read_global(hierarchy, child);
	assert(memcmp(&childGlobal, &t, sizeof(cranm_transform_t)) == 0);

	cranh_destroy(hierarchy);

}

#define cranberry_tests() test()

#else
#define cranberry_tests()
#endif // CRANBERRY_ENABLE_TESTS

const uint16_t window_Width = 1024;
const uint16_t window_Height = 720;
const char* window_Title = "Tower Fight?";

static uint64_t time_LastFrame = 0;

const uint8_t render_SampleCount = 4;
static sg_draw_state render_DrawState;

#define render_MaxInstanceCount 5000000
game_instance_t render_InstanceBuffer[render_MaxInstanceCount];

typedef struct
{
	float aspect;
	cranm_mat4x4_t viewProjection;
} render_params_t;

// TODO: Actually make the rendering work
void core_init(void)
{
	Mist_ProfileInit();

	stm_setup();

	sg_desc desc = { 0 };
	sg_setup(&desc);

	sg_buffer instanceBuffer = sg_make_buffer(&(sg_buffer_desc)
	{
		.type = SG_BUFFERTYPE_VERTEXBUFFER,
		.size = sizeof(game_instance_t) * render_MaxInstanceCount,
		.usage = SG_USAGE_STREAM
	});

	uint16_t indices[] = 
	{ 
		0, 1, 2, 2, 1, 3, // front
		1, 4, 3, 3, 4, 5, // right
		4, 6, 5, 5, 6, 7, // back
		6, 0, 7, 7, 0, 2, // left
		2, 3, 7, 7, 3, 5, // top
		6, 4, 0, 0, 4, 1  // bottom
	};
	sg_buffer indexBuffer = sg_make_buffer(&(sg_buffer_desc)
	{
		.type = SG_BUFFERTYPE_INDEXBUFFER,
		.size = sizeof(uint16_t) * 36,
		.content = indices
	});

	sg_shader shader = sg_make_shader(&(sg_shader_desc)
	{
		.fs = { .source = shader_FragDefault3D },
		.vs =
		{
			.uniform_blocks[0] =
			{
				.size = sizeof(render_params_t),
				.uniforms[0] = {.name = "aspect",.type = SG_UNIFORMTYPE_FLOAT },
				.uniforms[1] = {.name = "viewProjection",.type = SG_UNIFORMTYPE_MAT4 }
			},
			.source = shader_VertDefault3D
		}
	});

	sg_pipeline pipeline = sg_make_pipeline(&(sg_pipeline_desc)
	{
		.layout =
		{
			.buffers[0] = {.step_func = SG_VERTEXSTEP_PER_INSTANCE },
			.attrs = 
			{ 
				[0] = (sg_vertex_attr_desc) {.name = "rotation", .format = SG_VERTEXFORMAT_FLOAT4, .offset = offsetof(game_instance_t, transform) + offsetof(cranm_transform_t, rot), .stride = sizeof(game_instance_t)},
				[1] = (sg_vertex_attr_desc) { .name = "position",.format = SG_VERTEXFORMAT_FLOAT3, .offset = offsetof(game_instance_t, transform) + offsetof(cranm_transform_t, pos), .stride = sizeof(game_instance_t) },
				[2] = (sg_vertex_attr_desc) { .name = "scale",.format = SG_VERTEXFORMAT_FLOAT,.offset = offsetof(game_instance_t, transform) + offsetof(cranm_transform_t, scale), .stride = sizeof(game_instance_t) },
				[3] = (sg_vertex_attr_desc) { .name = "color",.format = SG_VERTEXFORMAT_FLOAT3,.offset = offsetof(game_instance_t, color),.stride = sizeof(game_instance_t) }
			}
		},
		.shader = shader,
		.index_type = SG_INDEXTYPE_UINT16,
		.depth_stencil =
		{
			.depth_compare_func = SG_COMPAREFUNC_LESS,
			.depth_write_enabled = true
		},

		.rasterizer = { .cull_mode = SG_CULLMODE_NONE,.sample_count = render_SampleCount },
		.blend =
		{
			.enabled = true,
			.src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA,
			.dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
			.src_factor_alpha = SG_BLENDFACTOR_SRC_ALPHA,
			.dst_factor_alpha = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA
		}
	});

	render_DrawState = (sg_draw_state)
	{
		.pipeline = pipeline,
		.index_buffer = indexBuffer,
		.vertex_buffers[0] = instanceBuffer
	};

	game_init();
}

void core_frame(void)
{
	int width = sapp_width();
	int height = sapp_height();

	uint64_t lap = stm_laptime(&time_LastFrame);
	game_tick(0.016f);

	unsigned int instanceCount = game_gen_instance_buffer(render_InstanceBuffer, render_MaxInstanceCount);
	sg_update_buffer(render_DrawState.vertex_buffers[0], render_InstanceBuffer, instanceCount * sizeof(game_instance_t));

	sg_pass_action passAction =
	{
		.colors[0] = {.action = SG_ACTION_CLEAR,.val = { 0.1f, 0.1f, 0.1f, 1.0f } }
	};
	sg_begin_default_pass(&passAction, (int)width, (int)height);
	sg_apply_draw_state(&render_DrawState);
	sg_apply_uniform_block(SG_SHADERSTAGE_VS, 0, &(render_params_t){.aspect = (float)width / height, .viewProjection = cranm_perspective(0.1f, 100.0f, 90.0f)}, sizeof(render_params_t));
	sg_draw(0, 36, instanceCount);
	sg_end_pass();
	sg_commit();

}

void core_cleanup(void)
{
	game_cleanup();

	sg_shutdown();

	FILE* fileHandle = fopen("game_chrome_trace.json", "w");

	char* print = 0;
	size_t bufferSize = 0;
	Mist_FlushThreadBuffer();
	Mist_FlushAlloc(&print, &bufferSize);

	fprintf(fileHandle, "%s", mist_ProfilePreface);
	fprintf(fileHandle, "%s", print);
	fprintf(fileHandle, "%s", mist_ProfilePostface);

	Mist_Free(print);
	fclose(fileHandle);
	Mist_ProfileTerminate();
}

sapp_desc sokol_main(int argc, char* argv[])
{
	cranberry_tests();

	return (sapp_desc)
	{
		.init_cb = core_init,
		.frame_cb = core_frame,
		.cleanup_cb = core_cleanup,
		.width = window_Width,
		.height = window_Height,
		.window_title = window_Title,
	};
}
