#version 330

layout(location = 0) in vec4 a_position;
// layout(location = 1) in vec3 a_color;

uniform mat4 u_mvp;

void main()
{
    gl_Position = u_mvp * a_position;
}
