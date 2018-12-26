#version 330

uniform mat4 u_viewProjection;
uniform float u_scale;

layout(location = 0) in vec4 a_position;
layout(location = 1) in vec4 a_center;

void main()
{
    mat4 modelMatrix = mat4(u_scale);
    modelMatrix[3] = a_center;
    modelMatrix[3][3] = 1;

    gl_Position = u_viewProjection * modelMatrix * a_position;
}
