#version 330

layout (location = 0) in vec2 a_pos;
layout (location = 1) in vec2 a_uv;
layout (location = 2) in vec4 a_color;

out vec2 v_uv;
out vec4 v_color;

uniform mat4 u_projectionMatrix;

void main()
{
    v_color = a_color;
    v_uv    = a_uv;
    gl_Position = u_projectionMatrix * vec4(a_pos, 0, 1);
}
