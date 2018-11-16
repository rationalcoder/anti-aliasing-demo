#version 330

in vec2 v_uv;
in vec4 v_color;

uniform sampler2D u_texture;

void main()
{
    gl_FragColor = v_color * texture(u_texture, v_uv).r; // Alpha8
    //gl_FragColor = v_color * texture(u_texture, v_uv.st); // RGBA32
}
