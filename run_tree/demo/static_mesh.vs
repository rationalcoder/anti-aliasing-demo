#version 330

layout(location = 0) in vec4 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_uv;

out vec3 v_pos;
out vec3 v_normal;
out vec2 v_uv;
out vec3 v_eye;
out vec3 v_lightDir;
out float v_lightDist;

uniform mat4 u_modelMatrix;
uniform mat4 u_viewMatrix;
uniform mat4 u_projectionMatrix;
uniform mat3 u_normalMatrix;

uniform vec3 u_pointLightP;

void main()
{
    vec4 mvPos = u_viewMatrix * u_modelMatrix * a_position;
    v_pos    = mvPos.xyz;
    v_uv     = a_uv;

    v_eye       = -v_pos;
    v_normal    = u_normalMatrix * a_normal;
    v_lightDir  = u_pointLightP - v_pos;
    v_lightDist = length(v_lightDir);

    gl_Position = u_projectionMatrix * mvPos;
}
