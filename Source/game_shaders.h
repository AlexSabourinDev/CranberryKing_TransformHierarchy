#pragma once

#ifndef RENDER_SHADER_STRINGIFY
#define RENDER_SHADER_STRINGIFY(x) #x
#endif // RENDER_SHADER_STRINGIFY

const char* shader_VertDefault3D = "#version 330\n" RENDER_SHADER_STRINGIFY
(

uniform float aspect;
uniform mat4x4 viewProjection;

in float scale;
in vec3 position;
in vec4 rotation;
in vec3 color;
out vec3 out_norm;
out vec3 out_color;

vec3 cube[8] = vec3[](
	vec3(-1.0f, -1.0f, -1.0f),
	vec3(1.0f, -1.0f, -1.0f),
	vec3(-1.0f, 1.0f, -1.0f),
	vec3(1.0f, 1.0f, -1.0f),
	vec3(1.0f, -1.0f, 1.0f),
	vec3(1.0f, 1.0f, 1.0f),
	vec3(-1.0f, -1.0f, 1.0f),
	vec3(-1.0f, 1.0f, 1.0f)
	);

vec3 rotate(vec3 v, vec4 q)
{
	vec3 t = cross(2.0 * q.xyz, v);
	return v + q.w * t + cross(q.xyz, t);
}

void main()
{
	vec3 pos = rotate(cube[gl_VertexID] * scale, rotation) + position;
	gl_Position = viewProjection * vec4(pos, 1.0);
	out_norm = pos;
	out_color = color * (dot(normalize(cube[gl_VertexID]), vec3(0.707, 0.707, 0.0f)) + 1.0f) * 0.75f + 0.25f;
}
);

const char* shader_FragDefault3D = "#version 330\n" RENDER_SHADER_STRINGIFY
(
in vec3 out_color;
in vec3 out_norm;
out vec4 frag_color;
void main()
{
	frag_color = vec4(out_color * (dot(normalize(out_norm), vec3(0.707, 0.707, 0.0f)) * 0.5 + 0.5), 1.0);
}
);

