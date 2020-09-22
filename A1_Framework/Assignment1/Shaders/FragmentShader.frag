#version 430

in vec3 clr_from_vtxshader;
out vec4 clr_from_fragshader;

void main()
{
	clr_from_fragshader = vec4(clr_from_vtxshader + 0.5, 1.f);
}