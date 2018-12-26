#version 330

layout(location = 0) in vec4 a_position;
layout(location = 1) in vec2 a_uv;
layout(location = 2) in vec3 a_normal;
layout(location = 3) in vec3 a_tangent;

out vec3 v_pos;
out vec3 v_normal;
out vec2 v_uv;
out vec3 v_eye;
out vec3 v_lightP;
out float v_lightDist;

uniform mat4 u_modelViewMatrix;
uniform mat4 u_projectionMatrix;
uniform mat3 u_normalMatrix;

uniform vec3 u_pointLightP;
uniform bool u_hasNormalMap;

void normal_map()
{
    vec3 T = normalize(u_normalMatrix * a_tangent);
    vec3 N = normalize(u_normalMatrix * a_normal);
    vec3 B = cross(N, T);

    mat3 ITBN = transpose(mat3(T, B, N));

    vec4 mvPos = u_modelViewMatrix * a_position;
    v_normal   = N; // FS probably doesn't need this, but whatever.
    v_uv       = a_uv;

    v_pos       = ITBN * mvPos.xyz;
    v_lightP    = ITBN * u_pointLightP;
    v_eye       = -v_pos;
    v_lightDist = length(v_lightP - v_pos);

    gl_Position = u_projectionMatrix * mvPos;
}

void standard()
{
    vec4 mvPos = u_modelViewMatrix * a_position;
    v_pos      = mvPos.xyz;
    v_uv       = a_uv;

    v_eye      = -v_pos;
    v_normal   = u_normalMatrix * a_normal;
    v_lightP   = u_pointLightP;
    v_lightDist = length(v_lightP - v_pos);

    gl_Position = u_projectionMatrix * mvPos;
}

void main()
{
    if (u_hasNormalMap) {
        normal_map();
    }
    else {
        standard();
    }
}
