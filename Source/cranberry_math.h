#ifndef __CRANBERRY_MATH_H
#define __CRANBERRY_MATH_H

// Types

typedef struct
{
	float x, y, z;
} cranm_vec3_t;

typedef struct
{
	float x, y, z, w;
} cranm_quat_t;

typedef struct
{
	cranm_quat_t rot;
	cranm_vec3_t pos;
	cranm_vec3_t scale;
} cranm_transform_t;

typedef struct
{
	float m[16];
} cranm_mat4x4_t;

// API

cranm_vec3_t cranm_add3(cranm_vec3_t l, cranm_vec3_t r);
cranm_vec3_t cranm_scale(cranm_vec3_t l, float s);
cranm_vec3_t cranm_scale3(cranm_vec3_t l, cranm_vec3_t r);
cranm_vec3_t cranm_cross(cranm_vec3_t l, cranm_vec3_t r);

cranm_vec3_t cranm_quat_t_xyz(cranm_quat_t q);
cranm_quat_t cranm_mulq(cranm_quat_t l, cranm_quat_t r);
cranm_vec3_t cranm_rot3(cranm_vec3_t v, cranm_quat_t r);

cranm_mat4x4_t cranm_identity4x4();
cranm_mat4x4_t cranm_mul4x4(cranm_mat4x4_t l, cranm_mat4x4_t r);

cranm_transform_t cranm_transform(cranm_transform_t t, cranm_transform_t by);

// IMPL

static cranm_vec3_t cranm_add3(cranm_vec3_t l, cranm_vec3_t r)
{
	return (cranm_vec3_t) { .x = l.x + r.x, l.y + r.y, l.z + r.z };
}

static cranm_vec3_t cranm_scale(cranm_vec3_t l, float s)
{
	return (cranm_vec3_t) { .x = l.x * s, .y = l.y * s, .z = l.z * s };
}

static cranm_vec3_t cranm_scale3(cranm_vec3_t l, cranm_vec3_t r)
{
	return (cranm_vec3_t) { .x = l.x * r.x, .y = l.y * r.y, .z = l.z * r.z };
}

static cranm_vec3_t cranm_cross(cranm_vec3_t l, cranm_vec3_t r)
{
	return (cranm_vec3_t) { .x = l.y * r.z - l.z * r.y, .y = l.z * r.x - l.x * r.z, .z = l.x * r.y - l.y * r.x };
}

static cranm_vec3_t cranm_quat_t_xyz(cranm_quat_t q)
{
	return (cranm_vec3_t) { .x = q.x, .y = q.y, .z = q.z };
}

static cranm_quat_t cranm_mulq(cranm_quat_t l, cranm_quat_t r)
{
	return (cranm_quat_t)
	{
		.w = l.w * r.w - l.x * r.x - l.y * r.y - l.z * r.z,
		.x = l.w * r.x + l.x * r.w - l.y * r.z + l.z * r.y,
		.y = l.w * r.y + l.x * r.z + l.y * r.w - l.z * r.x,
		.z = l.w * r.z - l.x * r.y + l.y * r.x + l.z * r.w
	};
}

static cranm_vec3_t cranm_rot3(cranm_vec3_t v, cranm_quat_t r)
{
	cranm_vec3_t t = cranm_scale(cranm_quat_t_xyz(r), 2.0f);
	t = cranm_cross(t, v);

	cranm_vec3_t res = cranm_add3(v, cranm_scale(t, r.w));
	return cranm_add3(res, cranm_cross(cranm_quat_t_xyz(r), t));
}

static cranm_mat4x4_t cranm_identity4x4()
{
	cranm_mat4x4_t mat;
	mat.m[0] = mat.m[5] = mat.m[10] = mat.m[15] = 1.0f;
	return mat;
}

static cranm_mat4x4_t cranm_mul4x4(cranm_mat4x4_t l, cranm_mat4x4_t r)
{
	cranm_mat4x4_t mat;

	for (unsigned int i = 0; i < 4; ++i)
	{
		float l0 = l.m[i * 4 + 0];
		float l1 = l.m[i * 4 + 1];
		float l2 = l.m[i * 4 + 2];
		float l3 = l.m[i * 4 + 3];

		mat.m[i * 4 + 0] = l0 * r.m[0] + l1 * r.m[4] + l2 * r.m[8] + l3 * r.m[12];
		mat.m[i * 4 + 1] = l0 * r.m[1] + l1 * r.m[5] + l2 * r.m[9] + l3 * r.m[13];
		mat.m[i * 4 + 2] = l0 * r.m[2] + l1 * r.m[6] + l2 * r.m[10] + l3 * r.m[14];
		mat.m[i * 4 + 3] = l0 * r.m[3] + l1 * r.m[7] + l2 * r.m[11] + l3 * r.m[15];
	}

	return mat;
}

static cranm_transform_t cranm_transform(cranm_transform_t t, cranm_transform_t by)
{
	return (cranm_transform_t)
	{
		.rot = cranm_mulq(t.rot, by.rot),
		.scale = cranm_scale3(t.scale, by.scale),
		.pos = cranm_add3(cranm_rot3(cranm_scale3(t.pos, by.scale), by.rot), by.pos)
	};
}

#endif // __CRANBERRY_MATH_H
