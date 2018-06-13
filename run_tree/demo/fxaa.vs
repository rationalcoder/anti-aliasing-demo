#version 430 core

out vec2 v_texCoord;

void main(void)
{
    // No need to actually pass any vertices. Just use glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    vec4 vertices[4] = vec4[4](
        vec4(-1.0, -1.0, 0.0, 1.0), 
        vec4(1.0, -1.0, 0.0, 1.0), 
        vec4(-1.0, 1.0, 0.0, 1.0), 
        vec4(1.0, 1.0, 0.0, 1.0));

    vec2 texCoord[4] = vec2[4](vec2(0.0, 0.0), vec2(1.0, 0.0), vec2(0.0, 1.0), vec2(1.0, 1.0));

    v_texCoord  = texCoord[gl_VertexID];
    gl_Position = vertices[gl_VertexID];
}
