#ifndef __CRANBERRY_MATH_H
#define __CRANBERRY_MATH_H

#include <math.h>

#define cranm_batch_size 4

#ifdef CRANBERRY_SSE
#include <immintrin.h>
#include <emmintrin.h>

#define cranm_shuffle_sse(a, b) _mm_castsi128_ps(_mm_shuffle_epi32(_mm_castps_si128(a), b))

#endif // CRANBERRY_SSE

#ifdef CRANBERRY_DEBUG
#include <assert.h>
#endif // CRANBERRY_DEBUG

// Types

typedef struct
{
	float x, y, z, w;
} cranm_vec_t;

typedef struct
{
	float x, y, z, w;
} cranm_quat_t;

typedef struct
{
	cranm_quat_t rot;
	cranm_vec_t pos;
	cranm_vec_t scale;
} cranm_transform_t;

typedef struct
{
	float x[cranm_batch_size];
	float y[cranm_batch_size];
	float z[cranm_batch_size];
} cranm_batch_vec3_t;

typedef struct
{
	float x[cranm_batch_size];
	float y[cranm_batch_size];
	float z[cranm_batch_size];
	float w[cranm_batch_size];
} cranm_batch_quat_t;

typedef struct
{
	cranm_batch_vec3_t pos;
	cranm_batch_quat_t rot;
	cranm_batch_vec3_t scale;
} cranm_batch_transform_t __attribute__((aligned(16)));

typedef struct
{
	float m[16];
} cranm_mat4x4_t;

// API

static cranm_vec_t cranm_add3(cranm_vec_t l, cranm_vec_t r);
static cranm_vec_t cranm_sub3(cranm_vec_t l, cranm_vec_t r);
static cranm_vec_t cranm_scale(cranm_vec_t l, float s);
static cranm_vec_t cranm_scale3(cranm_vec_t l, cranm_vec_t r);
static cranm_vec_t cranm_cross(cranm_vec_t l, cranm_vec_t r);
static cranm_vec_t cranm_normalize3(cranm_vec_t v);

static cranm_vec_t cranm_quat_t_xyz(cranm_quat_t q);
static cranm_quat_t cranm_axis_angleq(cranm_vec_t axis, float angle);
static cranm_quat_t cranm_mulq(cranm_quat_t l, cranm_quat_t r);
static cranm_quat_t cranm_inverse_mulq(cranm_quat_t l, cranm_quat_t r);
static cranm_quat_t cranm_inverseq(cranm_quat_t q);
static cranm_vec_t cranm_rot3(cranm_vec_t v, cranm_quat_t r);
static cranm_vec_t cranm_inverse_rot3(cranm_vec_t v, cranm_quat_t r);

static cranm_mat4x4_t cranm_identity4x4();
static cranm_mat4x4_t cranm_mul4x4(cranm_mat4x4_t l, cranm_mat4x4_t r);
static cranm_mat4x4_t cranm_perspective(float near, float far, float fov);

static cranm_transform_t cranm_batch_to_single_transform(cranm_batch_transform_t* transform, unsigned int index);
static void cranm_insert_single_into_batch(cranm_batch_transform_t* batch, cranm_transform_t* transform, unsigned int index);
static void cranm_batch_transform(cranm_batch_transform_t* out, cranm_batch_transform_t* t, cranm_batch_transform_t* by);

static cranm_transform_t cranm_transform(cranm_transform_t* t, cranm_transform_t* by);
static cranm_transform_t cranm_inverse_transform(cranm_transform_t* t, cranm_transform_t* by);

// IMPL

static cranm_vec_t cranm_add3(cranm_vec_t l, cranm_vec_t r)
{
#ifdef CRANBERRY_SSE
	__m128 lv = _mm_load_ps((float*)&l);
	__m128 rv = _mm_load_ps((float*)&r);

	cranm_vec_t result;
	_mm_store_ps((float*)&result, _mm_add_ps(lv, rv));
	return result;
#else
	return (cranm_vec_t) { .x = l.x + r.x, l.y + r.y, l.z + r.z };
#endif // CRANBERRY_SSE
}

static cranm_vec_t cranm_sub3(cranm_vec_t l, cranm_vec_t r)
{
	return (cranm_vec_t) { .x = l.x - r.x, l.y - r.y, l.z - r.z };
}

static cranm_vec_t cranm_scale(cranm_vec_t l, float s)
{
#ifdef CRANBERRY_SSE
	__m128 sv = _mm_set1_ps(s);
	__m128 lv = _mm_load_ps((float*)&l);

	cranm_vec_t result;
	_mm_store_ps((float*)&result, _mm_mul_ps(sv, lv));
	return result;
#else
	return (cranm_vec_t) { .x = l.x * s, .y = l.y * s, .z = l.z * s };
#endif // CRANBERRY_SSE
}

static cranm_vec_t cranm_scale3(cranm_vec_t l, cranm_vec_t r)
{
#ifdef CRANBERRY_SSE
	__m128 lv = _mm_load_ps((float*)&l);
	__m128 rv = _mm_load_ps((float*)&r);

	cranm_vec_t result;
	_mm_store_ps((float*)&result, _mm_mul_ps(lv, rv));
	return result;
#else
	return (cranm_vec_t) { .x = l.x * r.x, .y = l.y * r.y, .z = l.z * r.z };
#endif // CRANBERRY_SSE
}

static cranm_vec_t cranm_cross(cranm_vec_t l, cranm_vec_t r)
{
#ifdef CRANBERRY_SSE
	__m128 lv = _mm_load_ps((float*)&l);
	__m128 rv = _mm_load_ps((float*)&r);

	__m128 l1 = cranm_shuffle_sse(lv, _MM_SHUFFLE(0, 0, 2, 1));
	__m128 l2 = cranm_shuffle_sse(lv, _MM_SHUFFLE(0, 1, 0, 2));

	__m128 r1 = cranm_shuffle_sse(rv, _MM_SHUFFLE(0, 1, 0, 2));
	__m128 r2 = cranm_shuffle_sse(rv, _MM_SHUFFLE(0, 0, 2, 1));
	
	__m128 lm = _mm_mul_ps(l1, r1);
	__m128 rm = _mm_mul_ps(l2, r2);
	cranm_vec_t result;
	_mm_store_ps((float*)&result, _mm_sub_ps(lm, rm));

	return result;
#else
	return (cranm_vec_t) 
	{ 
		.x = l.y * r.z - l.z * r.y,
		.y = l.z * r.x - l.x * r.z,
		.z = l.x * r.y - l.y * r.x
	};
#endif // CRANBERRY_SSE
}

static cranm_vec_t cranm_normalize3(cranm_vec_t v)
{
	float rm = 1.0f / sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
	return (cranm_vec_t) { .x = v.x * rm, .y = v.y * rm, .z = v.z * rm };
}

static cranm_vec_t cranm_quat_t_xyz(cranm_quat_t q)
{
	return (cranm_vec_t) { .x = q.x, .y = q.y, .z = q.z, .w = 0.0f };
}

static cranm_quat_t cranm_mulq(cranm_quat_t l, cranm_quat_t r)
{
#ifdef CRANBERRY_SSE
	__m128 q = _mm_load_ps((float*)&r);
	__m128 s = _mm_load_ps((float*)&l);

	__m128 w = cranm_shuffle_sse(s, _MM_SHUFFLE(3, 3, 3, 3));
	__m128 x = cranm_shuffle_sse(s, _MM_SHUFFLE(0, 0, 0, 0));
	__m128 y = cranm_shuffle_sse(s, _MM_SHUFFLE(1, 1, 1, 1));
	__m128 z = cranm_shuffle_sse(s, _MM_SHUFFLE(2, 2, 2, 2));

	__m128 rw = _mm_mul_ps(w, q);
	__m128 rx = _mm_mul_ps(x, cranm_shuffle_sse(q, _MM_SHUFFLE(0, 1, 2, 3)));
	__m128 ry = _mm_mul_ps(y, cranm_shuffle_sse(q, _MM_SHUFFLE(1, 0, 3, 2)));
	__m128 rz = _mm_mul_ps(z, cranm_shuffle_sse(q, _MM_SHUFFLE(2, 3, 0, 1)));

	__m128 f = _mm_add_ps(rw, _mm_xor_ps(rx, _mm_set_ps(-0.0f, -0.0f, 0.0f, 0.0f)));
	f = _mm_add_ps(f, _mm_xor_ps(ry, _mm_set_ps(-0.0f, 0.0f, 0.0f, -0.0f)));
	f = _mm_add_ps(f, _mm_xor_ps(rz, _mm_set_ps(-0.0f, 0.0f, -0.0f, 0.0f)));

	cranm_quat_t result;
	_mm_store_ps((float*)&result, f);

#ifdef CRANBERRY_DEBUG
	cranm_quat_t test = 
	{
		.x = l.w * r.x + l.x * r.w - l.y * r.z + l.z * r.y,
		.y = l.w * r.y + l.x * r.z + l.y * r.w - l.z * r.x,
		.z = l.w * r.z - l.x * r.y + l.y * r.x + l.z * r.w,
		.w = l.w * r.w - l.x * r.x - l.y * r.y - l.z * r.z
	};

	assert(result.x == test.x && result.y == test.y && result.z == test.z && result.w == test.w);
#endif // CRANBERRY_DEBUG

	return result;
#else
	return (cranm_quat_t)
	{
		.x = l.w * r.x + l.x * r.w - l.y * r.z + l.z * r.y,
		.y = l.w * r.y + l.x * r.z + l.y * r.w - l.z * r.x,
		.z = l.w * r.z - l.x * r.y + l.y * r.x + l.z * r.w,
		.w = l.w * r.w - l.x * r.x - l.y * r.y - l.z * r.z
	};
#endif // CRANBERRY_SSE
}

static cranm_quat_t cranm_inverse_mulq(cranm_quat_t l, cranm_quat_t r)
{
#ifdef CRANBERRY_SSE
	__m128 q = _mm_load_ps((float*)&r);
	__m128 s = _mm_load_ps((float*)&l);

	__m128 w = cranm_shuffle_sse(s, _MM_SHUFFLE(3, 3, 3, 3));
	w = _mm_xor_ps(w, _mm_set_ps(0.0f, -0.0f, -0.0f, -0.0f));

	__m128 x = cranm_shuffle_sse(s, _MM_SHUFFLE(0, 0, 0, 0));
	__m128 y = cranm_shuffle_sse(s, _MM_SHUFFLE(1, 1, 1, 1));
	__m128 z = cranm_shuffle_sse(s, _MM_SHUFFLE(2, 2, 2, 2));

	__m128 rw = _mm_mul_ps(w, q);
	__m128 rx = _mm_mul_ps(x, cranm_shuffle_sse(q, _MM_SHUFFLE(0, 1, 2, 3)));
	__m128 ry = _mm_mul_ps(y, cranm_shuffle_sse(q, _MM_SHUFFLE(1, 0, 3, 2)));
	__m128 rz = _mm_mul_ps(z, cranm_shuffle_sse(q, _MM_SHUFFLE(2, 3, 0, 1)));

	__m128 f = _mm_add_ps(rw, _mm_xor_ps(rx, _mm_set_ps(0.0f, 0.0f, -0.0f, 0.0f)));
	f = _mm_add_ps(f, _mm_xor_ps(ry, _mm_set_ps(0.0f, -0.0f, 0.0f, 0.0f)));
	f = _mm_add_ps(f, _mm_xor_ps(rz, _mm_set_ps(0.0f, 0.0f, 0.0f, -0.0f)));

	cranm_quat_t result;
	_mm_store_ps((float*)&result, f);

#ifdef CRANBERRY_DEBUG
	cranm_quat_t test =
	{
		.x = -l.w * r.x + l.x * r.w + l.y * r.z - l.z * r.y,
		.y = -l.w * r.y - l.x * r.z + l.y * r.w + l.z * r.x,
		.z = -l.w * r.z + l.x * r.y - l.y * r.x + l.z * r.w,
		.w =  l.w * r.w + l.x * r.x + l.y * r.y + l.z * r.z
	};

	assert(result.x == test.x && result.y == test.y && result.z == test.z && result.w == test.w);
#endif // CRANBERRY_DEBUG

	return result;
#else
	return (cranm_quat_t)
	{
		.x = -l.w * r.x + l.x * r.w + l.y * r.z - l.z * r.y,
		.y = -l.w * r.y - l.x * r.z + l.y * r.w + l.z * r.x,
		.z = -l.w * r.z + l.x * r.y - l.y * r.x + l.z * r.w,
		.w =  l.w * r.w + l.x * r.x + l.y * r.y + l.z * r.z
	};
#endif // CRANBERRY_SSE
}

static cranm_quat_t cranm_axis_angleq(cranm_vec_t axis, float angle)
{
	float cr = cosf(angle * 0.5f);
	float sr = sinf(angle * 0.5f);
	return (cranm_quat_t) { .w = cr, .x = axis.x * sr, .y = axis.y * sr, .z = axis.z * sr };
}

static cranm_quat_t cranm_inverseq(cranm_quat_t q)
{
	return (cranm_quat_t) { .x = -q.x, .y = -q.y, .z = -q.z, .w = q.w };
}

static cranm_vec_t cranm_rot3(cranm_vec_t v, cranm_quat_t r)
{
	cranm_vec_t t = cranm_scale(cranm_quat_t_xyz(r), 2.0f);
	t = cranm_cross(t, v);

	cranm_vec_t res = cranm_add3(v, cranm_scale(t, r.w));
	return cranm_add3(res, cranm_cross(cranm_quat_t_xyz(r), t));
}

static cranm_vec_t cranm_inverse_rot3(cranm_vec_t v, cranm_quat_t r)
{
	cranm_vec_t t = cranm_scale(cranm_quat_t_xyz(r), -2.0f);
	t = cranm_cross(t, v);

	cranm_vec_t res = cranm_add3(v, cranm_scale(t, r.w));
	return cranm_add3(res, cranm_cross(cranm_scale(cranm_quat_t_xyz(r), -1.0f), t));
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

static cranm_mat4x4_t cranm_perspective(float near, float far, float fov)
{
	float s = 1.0f / tanf(fov * 3.14159265358979f / 360.0f);

	cranm_mat4x4_t mat = { 0 };
	mat.m[0] = s;
	mat.m[5] = s;
	mat.m[10] = far / (far - near);
	mat.m[11] = 1.0f;
	mat.m[14] = -far * near / (far - near);

	return mat;
}

static cranm_transform_t cranm_batch_to_single_transform(cranm_batch_transform_t* transform, unsigned int index)
{
	return (cranm_transform_t)
	{
		.pos = { transform->pos.x[index], transform->pos.y[index], transform->pos.z[index]},
		.rot = { transform->rot.x[index], transform->rot.y[index], transform->rot.z[index], transform->rot.w[index]},
		.scale = { transform->scale.x[index], transform->scale.y[index], transform->scale.z[index]}
	};
}

static void cranm_insert_single_into_batch(cranm_batch_transform_t* batch, cranm_transform_t* transform, unsigned int index)
{
	batch->pos.x[index] = transform->pos.x;
	batch->pos.y[index] = transform->pos.y;
	batch->pos.z[index] = transform->pos.z;

	batch->rot.x[index] = transform->rot.x;
	batch->rot.y[index] = transform->rot.y;
	batch->rot.z[index] = transform->rot.z;
	batch->rot.w[index] = transform->rot.w;

	batch->scale.x[index] = transform->scale.x;
	batch->scale.y[index] = transform->scale.y;
	batch->scale.z[index] = transform->scale.z;
}

static void cranm_batch_transform(cranm_batch_transform_t* out, cranm_batch_transform_t* t, cranm_batch_transform_t* by)
{
	// Scale
	{
		_mm_store_ps(out->scale.x, _mm_mul_ps(_mm_load_ps(t->scale.x), _mm_load_ps(by->scale.x)));
		_mm_store_ps(out->scale.y, _mm_mul_ps(_mm_load_ps(t->scale.y), _mm_load_ps(by->scale.y)));
		_mm_store_ps(out->scale.z, _mm_mul_ps(_mm_load_ps(t->scale.z), _mm_load_ps(by->scale.z)));
	}

	// Rotation
	{
		__m128 trx = _mm_load_ps(t->rot.x);
		__m128 try = _mm_load_ps(t->rot.y);
		__m128 trz = _mm_load_ps(t->rot.z);
		__m128 trw = _mm_load_ps(t->rot.w);

		__m128 byrx = _mm_load_ps(by->rot.x);
		__m128 byry = _mm_load_ps(by->rot.y);
		__m128 byrz = _mm_load_ps(by->rot.z);
		__m128 byrw = _mm_load_ps(by->rot.w);

		/*
			.x = l.w * r.x + l.x * r.w - l.y * r.z + l.z * r.y,
			.y = l.w * r.y + l.x * r.z + l.y * r.w - l.z * r.x,
			.z = l.w * r.z - l.x * r.y + l.y * r.x + l.z * r.w,
			.w = l.w * r.w - l.x * r.x - l.y * r.y - l.z * r.z
		*/
		_mm_store_ps(out->rot.x, _mm_add_ps(_mm_sub_ps(_mm_add_ps(_mm_mul_ps(trw, byrx), _mm_mul_ps(trx, byrw)), _mm_mul_ps(try, byrz)), _mm_mul_ps(trz, byry)));
		_mm_store_ps(out->rot.y, _mm_sub_ps(_mm_add_ps(_mm_add_ps(_mm_mul_ps(trw, byry), _mm_mul_ps(trx, byrz)), _mm_mul_ps(try, byrw)), _mm_mul_ps(trz, byrx)));
		_mm_store_ps(out->rot.z, _mm_add_ps(_mm_add_ps(_mm_sub_ps(_mm_mul_ps(trw, byrz), _mm_mul_ps(trx, byry)), _mm_mul_ps(try, byrx)), _mm_mul_ps(trz, byrw)));
		_mm_store_ps(out->rot.w, _mm_sub_ps(_mm_sub_ps(_mm_sub_ps(_mm_mul_ps(trw, byrw), _mm_mul_ps(trx, byrx)), _mm_mul_ps(try, byry)), _mm_mul_ps(trz, byrz)));
	}

	// Position
	{
		__m128 byrx = _mm_load_ps(by->rot.x);
		__m128 byry = _mm_load_ps(by->rot.y);
		__m128 byrz = _mm_load_ps(by->rot.z);
		__m128 byrw = _mm_load_ps(by->rot.w);

		// Apply scale
		__m128 outx = _mm_mul_ps(_mm_load_ps(t->pos.x), _mm_load_ps(by->scale.x));
		__m128 outy = _mm_mul_ps(_mm_load_ps(t->pos.y), _mm_load_ps(by->scale.y));
		__m128 outz = _mm_mul_ps(_mm_load_ps(t->pos.z), _mm_load_ps(by->scale.z));

		// Apply rotation
		{
			/*
				// Cross
				.x = l.y * r.z - l.z * r.y,
				.y = l.z * r.x - l.x * r.z,
				.z = l.x * r.y - l.y * r.x

				// Rotate
				cranm_vec_t t = cranm_scale(cranm_quat_t_xyz(r), 2.0f);
				t = cranm_cross(t, v);

				cranm_vec_t res = cranm_add3(v, cranm_scale(t, r.w));
				return cranm_add3(res, cranm_cross(cranm_quat_t_xyz(r), t));
			*/

			// scale by 2
			// cranm_vec_t t = cranm_scale(cranm_quat_t_xyz(r), 2.0f);
			__m128 scale2 = _mm_set1_ps(2.0f);
			__m128 tx = _mm_mul_ps(byrx, scale2);
			__m128 ty = _mm_mul_ps(byry, scale2);
			__m128 tz = _mm_mul_ps(byrz, scale2);

			// cross scaled imaginary of quat with pos
			// t = cranm_cross(t, v);
			__m128 rtx = _mm_sub_ps(_mm_mul_ps(ty, outz), _mm_mul_ps(tz, outy));
			__m128 rty = _mm_sub_ps(_mm_mul_ps(tz, outx), _mm_mul_ps(tx, outz));
			__m128 rtz = _mm_sub_ps(_mm_mul_ps(tx, outy), _mm_mul_ps(ty, outx));

			// scale by w, add to pos
			// cranm_vec_t res = cranm_add3(v, cranm_scale(t, r.w));
			__m128 resx = _mm_add_ps(outx, _mm_mul_ps(rtx, byrw));
			__m128 resy = _mm_add_ps(outy, _mm_mul_ps(rty, byrw));
			__m128 resz = _mm_add_ps(outz, _mm_mul_ps(rtz, byrw));

			// cross imaginary of quat with previous cross
			// cranm_cross(cranm_quat_t_xyz(r), t)
			__m128 ax = _mm_sub_ps(_mm_mul_ps(byry, rtz), _mm_mul_ps(byrz, rty));
			__m128 ay = _mm_sub_ps(_mm_mul_ps(byrz, rtx), _mm_mul_ps(byrx, rtz));
			__m128 az = _mm_sub_ps(_mm_mul_ps(byrx, rty), _mm_mul_ps(byry, rtx));

			outx = _mm_add_ps(resx, ax);
			outy = _mm_add_ps(resy, ay);
			outz = _mm_add_ps(resz, az);
		}

		// Add translation
		_mm_store_ps(out->pos.x, _mm_add_ps(outx, _mm_load_ps(by->pos.x)));
		_mm_store_ps(out->pos.y, _mm_add_ps(outy, _mm_load_ps(by->pos.y)));
		_mm_store_ps(out->pos.z, _mm_add_ps(outz, _mm_load_ps(by->pos.z)));
	}
}

static cranm_transform_t cranm_transform(cranm_transform_t* t, cranm_transform_t* by)
{
	return (cranm_transform_t)
	{
		.rot = cranm_mulq(t->rot, by->rot),
		.pos = cranm_add3(cranm_rot3(cranm_scale3(t->pos, by->scale), by->rot), by->pos),
		.scale = cranm_scale3(t->scale, by->scale)
	};
}

static cranm_transform_t cranm_inverse_transform(cranm_transform_t* t, cranm_transform_t* by)
{
	cranm_vec_t inverseScale = { .x = 1.0f / by->scale.x, .y = 1.0f / by->scale.y, .z = 1.0f / by->scale.z };

	return (cranm_transform_t)
	{
		.rot = cranm_inverse_mulq(t->rot, by->rot),
		.pos = cranm_scale3(cranm_inverse_rot3(cranm_sub3(t->pos, by->pos), by->rot), inverseScale),
		.scale = cranm_scale3(t->scale, inverseScale)
	};
}

#endif // __CRANBERRY_MATH_H
