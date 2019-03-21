#ifndef __CRANBERRY_MATH_H
#define __CRANBERRY_MATH_H

typedef struct
{
	float m[16];
} cran_mat4x4;

static cran_mat4x4 cran_identity4x4()
{
	cran_mat4x4 mat;
	mat.m[0] = mat.m[5] = mat.m[10] = mat.m[15] = 1.0f;
	return mat;
} 

static cran_mat4x4 cran_mul4x4(cran_mat4x4 l, cran_mat4x4 r)
{
	cran_mat4x4 mat;

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

#endif // __CRANBERRY_MATH_H
