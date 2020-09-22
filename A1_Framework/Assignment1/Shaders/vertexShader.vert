#version 430

layout(location = 0) in vec3 pos_from_vtxbuffer;
layout(location = 1) in vec3 clr_from_vtxbuffer;
smooth out vec3 clr_from_vtxshader;
uniform mat4 uMVP;

void main()
{
	gl_Position = uMVP * vec4(pos_from_vtxbuffer, 1.f);
	clr_from_vtxshader = clr_from_vtxbuffer;
}