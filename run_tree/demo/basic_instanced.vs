#version 330

uniform mat4 u_viewProjection;

layout(location = 0) in vec4 a_position;
layout(location = 1) in vec4 a_center;

void main()
{
    mat4 modelMatrix = mat4(1);
    modelMatrix[3] = a_center;

    gl_Position = u_viewProjection * modelMatrix * a_position;
}
