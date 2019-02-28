//
//{ Utility

static inline b32
compile_shader_from_source(GLuint shader, buffer32 source)
{
    glShaderSource(shader, 1, (GLchar**)&source.data, (GLint*)&source.size);
    glCompileShader(shader);

    GLint status = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);

    return status == GL_TRUE;
}

static inline b32
create_and_link_program(GLuint vs, GLuint fs, GLuint* program)
{
    GLuint p = glCreateProgram();
    glAttachShader(p, vs);
    glAttachShader(p, fs);

    glLinkProgram(p);

    GLint status = GL_FALSE;
    glGetProgramiv(p, GL_LINK_STATUS, &status);

    *program = p;
    return status == GL_TRUE;
}

static inline b32
load_shader(const char* file, GLenum type, GLuint* shader)
{
    buffer32 source = read_file_buffer(file);
    if (!source) return false;

    GLuint s = glCreateShader(type);
    if (s == GL_INVALID_VALUE) return false;

    GLchar errorBuffer[256];
    if (!compile_shader_from_source(s, source)) {
        glGetShaderInfoLog(s, sizeof(errorBuffer), nullptr, errorBuffer);
        log_crit("Failed to compile \"%s\": %s\n", file, errorBuffer);
        return false;
    }

    *shader = s;
    return true;
}

static inline b32
load_program(const char* vsFile, const char* fsFile, GLuint* program)
{
    buffer32 vsSource = read_file_buffer(vsFile);
    if (!vsSource) return false;

    buffer32 fsSource = read_file_buffer(fsFile);
    if (!fsSource) return false;

    GLuint vs = GL_INVALID_VALUE;
    if (!load_shader(vsFile, GL_VERTEX_SHADER, &vs)) return false;
    defer( glDeleteShader(vs); );

    GLuint fs = GL_INVALID_VALUE;
    if (!load_shader(fsFile, GL_FRAGMENT_SHADER, &fs)) return false;
    defer( glDeleteShader(fs); );

    char errorBuffer[256];
    if (!create_and_link_program(vs, fs, program)) {
        glGetProgramInfoLog(*program, sizeof(errorBuffer), nullptr, errorBuffer);
        log_crit("Failed to link program from \"%s\" and \"%s\": %s\n", vsFile, fsFile, errorBuffer);
        return false;
    }

    return true;
}

static inline b32
load_lines_program(const Shader_Catalog& catalog, Lines_Program* program)
{
    if (!create_and_link_program(catalog.basicVertexShader, catalog.solidFramentShader, &program->id)) {
        log_crit("Failed to load lines program!\n");
        return false;
    }

    GLuint id = program->id;

    program->mvp   = glGetUniformLocation(id, "u_mvp");
    program->color = glGetUniformLocation(id, "u_color");

    return true;
}

static inline b32
load_cubes_program(const Shader_Catalog& catalog, Cubes_Program* program)
{
    if (!create_and_link_program(catalog.basicInstancedVertexShader, catalog.solidFramentShader, &program->id)) {
        log_crit("Failed to load cubes program.\n");
        return false;
    }

    GLuint id = program->id;

    program->viewProjection = glGetUniformLocation(id, "u_viewProjection");
    program->color          = glGetUniformLocation(id, "u_color");
    program->scale          = glGetUniformLocation(id, "u_scale");

    return true;
}

static inline b32
load_static_mesh_program(const Shader_Catalog& catalog, Static_Mesh_Program* program)
{
    if (!create_and_link_program(catalog.staticMeshVertexShader, catalog.staticMeshFragmentShader, &program->id)) {
        log_crit("Failed to load static mesh program.\n");
        return false;
    }

    GLuint id = program->id;

    program->modelViewMatrix  = glGetUniformLocation(id, "u_modelViewMatrix");
    program->projectionMatrix = glGetUniformLocation(id, "u_projectionMatrix");
    program->normalMatrix     = glGetUniformLocation(id, "u_normalMatrix");

    program->solid = glGetUniformLocation(id, "u_solid");
    program->color = glGetUniformLocation(id, "u_color");

    program->diffuseMap  = glGetUniformLocation(id, "u_diffuse");
    program->normalMap   = glGetUniformLocation(id, "u_normal");
    program->specularMap = glGetUniformLocation(id, "u_specular");

    program->hasNormalMap   = glGetUniformLocation(id, "u_hasNormalMap");
    program->hasSpecularMap = glGetUniformLocation(id, "u_hasSpecularMap");

    program->specularExp = glGetUniformLocation(id, "u_specularExp");
    program->lit         = glGetUniformLocation(id, "u_lit");
    program->lightPos    = glGetUniformLocation(id, "u_pointLightP");
    program->lightColor  = glGetUniformLocation(id, "u_pointLightC");

    return true;
}

static inline b32
load_fxaa_program(const Shader_Catalog& catalog, FXAA_Program* program)
{
    if (!create_and_link_program(catalog.fxaaVertexShader, catalog.fxaaFragmentShader, &program->id)) {
        log_crit("Failed to load fxaa program.\n");
        return false;
    }

    GLuint id = program->id;

    program->on        = glGetUniformLocation(id, "u_fxaaOn");
    program->showEdges = glGetUniformLocation(id, "u_showEdges");
    program->texelStep = glGetUniformLocation(id, "u_texelStep");

    program->lumaThreshold = glGetUniformLocation(id, "u_lumaThreshold");
    program->mulReduce     = glGetUniformLocation(id, "u_mulReduce");
    program->minReduce     = glGetUniformLocation(id, "u_minReduce");
    program->maxSpan       = glGetUniformLocation(id, "u_maxSpan");

    return true;
}


static inline GLuint
bind_debug_lines(Render_Debug_Lines* cmd)
{
    if (cmd->_staged) {
        GLuint vao = (GLuint)(umm)cmd->_staged;
        glBindVertexArray(vao);
        return vao;
    }

    GLuint newVao = GL_INVALID_VALUE;

    glGenVertexArrays(1, &newVao);
    glBindVertexArray(newVao);
    glEnableVertexAttribArray(0);

    GLuint vbo = GL_INVALID_VALUE;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, cmd->vertexCount * 3 * sizeof(f32), cmd->vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glDeleteBuffers(1, &vbo);
    glBindVertexArray(newVao); // left bound on return

    cmd->_staged = (void*)(umm)newVao;
    return newVao;
}

static inline GLuint
bind_debug_cubes(GLuint vbo, GLuint ebo, Render_Debug_Cubes* cmd)
{
    if (cmd->_staged) {
        GLuint vao = (GLuint)(umm)cmd->_staged;
        glBindVertexArray(vao);
        return vao;
    }

    GLuint newVao = GL_INVALID_VALUE;
    glGenVertexArrays(1, &newVao);
    glBindVertexArray(newVao);

    GLuint centerBuffer = GL_INVALID_VALUE;
    glGenBuffers(1, &centerBuffer);

    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);

    glEnableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER, centerBuffer);
    glBufferData(GL_ARRAY_BUFFER, cmd->count * 3 * sizeof(f32), cmd->centers, GL_STATIC_DRAW);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, 0);
    glVertexAttribDivisor(1, 1);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindVertexArray(newVao);

    cmd->_staged = (void*)(umm)newVao;
    return newVao;
}

static inline GLenum
to_gl_index_type(Index_Size size)
{
    switch (size) {
    case IndexSize_u8:  return GL_UNSIGNED_BYTE;
    case IndexSize_u16: return GL_UNSIGNED_SHORT;
    case IndexSize_u32: return GL_UNSIGNED_INT;
    }

    return GL_INVALID_ENUM;
}

struct GL_Format
{
    GLenum upload   = GL_INVALID_ENUM;
    GLenum internal = GL_INVALID_ENUM;
};

// TODO(blake): handle more than 8 bits ber channel.
static inline GL_Format
to_gl_format(Texture_Format format, bool srgb)
{
    GL_Format result;

    if (srgb) {
        switch (format) {
        case TextureFormat_Grey:
            result.upload   = GL_RED;
            result.internal = GL_R8;
            break;
        case TextureFormat_GreyAlpha:
            result.upload   = GL_RG;
            result.internal = GL_RG8;
            break;
        case TextureFormat_RGB:
            result.upload   = GL_RGB;
            result.internal = GL_SRGB8;
            break;
        case TextureFormat_RGBA:
            result.upload   = GL_RGBA;
            result.internal = GL_SRGB8_ALPHA8;
            break;
        }
    }
    else {
        switch (format) {
        case TextureFormat_Grey:
            result.upload   = GL_RED;
            result.internal = GL_R8;
            break;
        case TextureFormat_GreyAlpha:
            result.upload   = GL_RG;
            result.internal = GL_RG8;
            break;
        case TextureFormat_RGB:
            result.upload   = GL_RGB;
            result.internal = GL_RGB8;
            break;
        case TextureFormat_RGBA:
            result.upload   = GL_RGBA;
            result.internal = GL_RGBA8;
            break;
        }
    }

    return result;
}

enum Texture_Options
{
    TexOpt_None          = 0x0,
    TexOpt_SRGB          = 0x1,
    TexOpt_Mipmap        = 0x2,
    TexOpt_UnpackRowZero = 0x4,
};

static inline GLuint
stage_texture(Texture texture, u32 options, GLenum wrapType)
{
    if (!texture.data) return GL_INVALID_VALUE;

    GLuint handle = GL_INVALID_VALUE;
    glGenTextures(1, &handle);

    glBindTexture(GL_TEXTURE_2D, handle);

    if (options & TexOpt_UnpackRowZero)
        glPixelStorei(GL_TEXTURE_2D, 0);

    GL_Format format = to_gl_format(texture.format, options & TexOpt_SRGB);
    glTexImage2D(GL_TEXTURE_2D, 0, format.internal, texture.x, texture.y, 0, format.upload, GL_UNSIGNED_BYTE, texture.data);

    GLuint minFilter = GL_LINEAR;
    if (options & TexOpt_Mipmap) {
        glGenerateMipmap(GL_TEXTURE_2D);
        minFilter = GL_NEAREST_MIPMAP_LINEAR;
    }

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrapType);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrapType);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minFilter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glBindTexture(GL_TEXTURE_2D, 0);

    return handle;
}

static inline Staged_Static_Mesh*
stage_static_mesh(Render_Static_Mesh* cmd)
{
    if (cmd->_staged) return (Staged_Static_Mesh*)cmd->_staged;

    Static_Mesh& mesh = cmd->mesh;

    GLuint vao = GL_INVALID_VALUE;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    GLuint vertices = GL_INVALID_VALUE;
    glGenBuffers(1, &vertices);

    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, vertices);
    glBufferData(GL_ARRAY_BUFFER, mesh.vertexCount * 3 * sizeof(f32), mesh.vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);

    GLuint uvs = GL_INVALID_VALUE;
    glGenBuffers(1, &uvs);

    glEnableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER, uvs);
    glBufferData(GL_ARRAY_BUFFER, mesh.vertexCount * 2 * sizeof(f32), mesh.uvs, GL_STATIC_DRAW);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, 0);

    if (mesh.has_normals()) {
        GLuint normals = GL_INVALID_VALUE;
        glGenBuffers(1, &normals);

        glEnableVertexAttribArray(2);
        glBindBuffer(GL_ARRAY_BUFFER, normals);
        glBufferData(GL_ARRAY_BUFFER, mesh.vertexCount * 3 * sizeof(f32), mesh.normals, GL_STATIC_DRAW);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 0, 0);
    }

    if (mesh.has_tangents()) {
        GLuint tangents = GL_INVALID_VALUE;
        glGenBuffers(1, &tangents);

        glEnableVertexAttribArray(3);
        glBindBuffer(GL_ARRAY_BUFFER, tangents);
        glBufferData(GL_ARRAY_BUFFER, mesh.vertexCount * 3 * sizeof(f32), mesh.tangents, GL_STATIC_DRAW);
        glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, 0, 0);
    }


    GLuint ebo = GL_INVALID_VALUE;
    glGenBuffers(1, &ebo);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh.indexCount * mesh.indexSize, mesh.indices, GL_STATIC_DRAW);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    // Textures

    u32 groupCount = mesh.material->coloredGroupCount;

    Staged_Static_Mesh* stagedMesh = allocate_new(Staged_Static_Mesh);
    stagedMesh->vao        = vao;
    stagedMesh->groups     = allocate_new_array(groupCount, Staged_Colored_Index_Group);
    stagedMesh->groupCount = groupCount;

    for (u32 i = 0; i < mesh.material->coloredGroupCount; i++) {
        Colored_Index_Group&        group       = mesh.material->coloredIndexGroups[i];
        Staged_Colored_Index_Group& stagedGroup = stagedMesh->groups[i];

        stagedGroup.indexStart = group.start;
        stagedGroup.indexCount = group.count;
        stagedGroup.indexType  = to_gl_index_type(mesh.indexSize);

        if (!group.has_diffuse_map()) {
            stagedGroup.color       = group.color;
            stagedGroup.specularExp = group.specularExp;
            continue;
        }

        stagedGroup.diffuseMap  = stage_texture(group.diffuseMap,  TexOpt_SRGB | TexOpt_Mipmap, GL_REPEAT);
        stagedGroup.emissiveMap = stage_texture(group.emissiveMap, TexOpt_SRGB | TexOpt_Mipmap, GL_REPEAT);
        stagedGroup.normalMap   = stage_texture(group.normalMap,                 TexOpt_Mipmap, GL_REPEAT);
        stagedGroup.specularMap = stage_texture(group.specularMap, TexOpt_SRGB | TexOpt_Mipmap, GL_REPEAT);
        stagedGroup.specularExp = group.specularExp;
    }

    cmd->_staged = stagedMesh;

    return stagedMesh;
}

static inline b32
load_debug_cube_buffers(GLuint* vertexBuffer, GLuint* indexBuffer)
{
    Static_Mesh cube = make_cube_mesh(.5f);

    GLuint vbo = GL_INVALID_VALUE;
    GLuint ebo = GL_INVALID_VALUE;

    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, cube.vertexCount * 3 * sizeof(f32), cube.vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, ebo);
    glBufferData(GL_ARRAY_BUFFER, cube.indexCount * cube.indexSize, cube.indices, GL_STATIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, 0);

    *vertexBuffer = vbo;
    *indexBuffer  = ebo;
    return true;
}

static inline b32
load_imgui_texture_atlas(ImGui_Resources* res)
{
    u8* out = nullptr;
    int w   = 0;
    int h   = 0;
    int bbp = 0;

    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->GetTexDataAsAlpha8(&out, &w, &h, &bbp);

    Texture tex;
    tex.data = out;
    tex.format = TextureFormat_Grey;
    tex.x = w;
    tex.y = h;

    GLuint atlasHandle = stage_texture(tex, TexOpt_UnpackRowZero, GL_CLAMP_TO_EDGE);
    res->textureAtlas = atlasHandle;
    io.Fonts->TexID   = (void*)(umm)atlasHandle;

    ImGui::MemFree(out);

    return true;
}

static inline b32
load_imgui(ImGui_Resources* res)
{
    ImGui_Program& program = res->program;

    GLuint vs = GL_INVALID_VALUE;
    GLuint fs = GL_INVALID_VALUE;
    if (!load_shader("assets/shaders/imgui.vs", GL_VERTEX_SHADER,   &vs)) return false;
    defer( glDeleteShader(vs); );

    if (!load_shader("assets/shaders/imgui.fs", GL_FRAGMENT_SHADER, &fs)) return false;
    defer( glDeleteShader(fs); );

    if (!create_and_link_program(vs, fs, &program.id)) return false;

    program.projectionMatrix = glGetUniformLocation(program.id, "u_projectionMatrix");
    program.texture          = glGetUniformLocation(program.id, "u_texture");

    glGenVertexArrays(1, &res->vao);

    glGenBuffers(1, &res->vertexBuffer);
    glGenBuffers(1, &res->indexBuffer);

    glBindVertexArray(res->vao);

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    if (!load_imgui_texture_atlas(res)) return false;

    return true;
}

static void
init_fxaa_pass(const FXAA_Program& program, Game_Resolution res, FXAA_Pass* pass)
{
    glGenVertexArrays(1, &pass->emptyVao);
    glBindVertexArray(pass->emptyVao);
    glBindVertexArray(0);

    glUseProgram(program.id);

    glUniform1i(program.on, pass->on);
    glUniform1i(program.showEdges, pass->showEdges);
    glUniform2f(program.texelStep, 1.0f/res.w, 1.0f/res.h);

    glUniform1f(program.lumaThreshold, pass->lumaThreshold);
    glUniform1f(program.minReduce, pass->minReduce);
    glUniform1f(program.mulReduce, pass->mulReduce);
    glUniform1f(program.maxSpan, pass->maxSpan);

    glUseProgram(0);
}

static void
free_fxaa_pass(FXAA_Pass* pass)
{
    glDeleteVertexArrays(1, &pass->emptyVao);
    pass->emptyVao = GL_INVALID_VALUE;
}

// NOTE(blake): doesn't support texture depth buffers, multiple attachments, etc. NBD for now.
// depthFormat == -1 => no depth buffer
// sampleCount ==  1 => no multisampling. Valid choices are 1, 2, 4, 8, and 16.
//
static inline b32
create_framebuffer(Game_Resolution res, GLint colorFormat, GLint depthFormat,
                   u32 sampleCount, Framebuffer* fb)
{
    assert(((sampleCount & (sampleCount-1)) == 0) && sampleCount <= 16);

    glGenFramebuffers(1, &fb->id);
    glGenTextures(1, &fb->color);
    if (depthFormat != -1)
        glGenRenderbuffers(1, &fb->depth);

    if (sampleCount == 1) {
        glBindTexture(GL_TEXTURE_2D, fb->color);
        // NOTE(blake): NULL data uses the pixel unpack buffer if one is bound.
        // We aren't using one at the moment, but still.
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
        glTexImage2D(GL_TEXTURE_2D, 0, colorFormat, res.w, res.h, 0, GL_RGBA /*N/A*/, GL_UNSIGNED_BYTE, NULL);

        glBindFramebuffer(GL_FRAMEBUFFER, fb->id);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fb->color, 0);

        if (depthFormat != -1) {
            glBindRenderbuffer(GL_RENDERBUFFER, fb->depth);
            glRenderbufferStorage(GL_RENDERBUFFER, depthFormat, res.w, res.h);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, fb->depth);
        }

        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE) {
            log_debug("Framebuffer incomplete! (GL Error: %x)\n", status);
            return false;
        }

        glBindTexture(GL_TEXTURE_2D, 0);
    }
    else {
        glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, fb->color);

        // NOTE(blake): NULL data uses the pixel unpack buffer if one is bound.
        // We aren't using one at the moment, but still.
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
        // NOTE(blake): if textures(read/write) and render buffers(read only) are mixed, fixedsamplelocations must be GL_TRUE!
        glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, sampleCount, colorFormat, res.w, res.h, GL_TRUE);

        glBindFramebuffer(GL_FRAMEBUFFER, fb->id);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, fb->color, 0);

        if (depthFormat != -1) {
            glBindRenderbuffer(GL_RENDERBUFFER, fb->depth);
            glRenderbufferStorageMultisample(GL_RENDERBUFFER, sampleCount, depthFormat, res.w, res.h);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, fb->depth);
        }

        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE) {
            log_debug("MSAA %dX framebuffer incomplete! (GL Error: %x)\n", sampleCount, status);
            return false;
        }

        glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, 0);
    }

    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    fb->sampleCount = sampleCount;
    return true;
}

static inline void
free_framebuffer(Framebuffer* fb)
{
    glDeleteTextures(1, &fb->color);
    glDeleteRenderbuffers(1, &fb->depth);
    glDeleteFramebuffers(1, &fb->id);

    fb->color = GL_INVALID_VALUE;
    fb->depth = GL_INVALID_VALUE;
    fb->id    = GL_INVALID_VALUE;
}

static inline b32
create_color_framebuffer(Game_Resolution res, GLint internalFormat, Framebuffer* fb)
{ return create_framebuffer(res, internalFormat, -1, 1, fb); }


static inline b32
load_msaa_pass(Game_Resolution res, u32 sampleCount, MSAA_Pass* pass)
{
    Framebuffer fb;
    if (!create_framebuffer(res, GL_RGB16F, GL_DEPTH_COMPONENT16, sampleCount, &fb))
        return false;

    pass->framebuffer = fb.id;
    pass->colorBuffer = fb.color;
    pass->depthBuffer = fb.depth;
    pass->sampleCount = sampleCount;
    return true;
}

static inline void
free_msaa_pass(MSAA_Pass* pass)
{
    assert(pass->sampleCount);

    glDeleteTextures(1, &pass->colorBuffer);
    glDeleteRenderbuffers(1, &pass->depthBuffer);
    glDeleteFramebuffers(1, &pass->framebuffer);

    pass->colorBuffer = GL_INVALID_VALUE;
    pass->depthBuffer = GL_INVALID_VALUE;
    pass->framebuffer = GL_INVALID_VALUE;
    pass->sampleCount = 0;
}

static inline void
render_msaa_pass_to_color_fbo(Memory_Arena* ws, const MSAA_Pass& pass, Game_Resolution res,
                              void* commands, u32 commandCount, Framebuffer* fb,
                              bool createNewFb = true)
{
    if (createNewFb) create_color_framebuffer(res, GL_SRGB8_ALPHA8, fb);

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, pass.framebuffer);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    renderer_exec(ws, commands, commandCount);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, pass.framebuffer);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fb->id);
    glBlitFramebuffer(0, 0, res.w, res.h, 0, 0, res.w, res.h, GL_COLOR_BUFFER_BIT, GL_LINEAR);
}

// Input color texture must be SRGB8_ALPHA8. Expect artefacts, otherise.
static inline void
render_fxaa_pass_to_color_fbo(const FXAA_Program& program, const FXAA_Pass& pass,
                              Game_Resolution res, GLuint colorTexure, Framebuffer* fb,
                              bool createNewFb = true, bool clearBackBuffer = false)
{
    if (createNewFb) create_color_framebuffer(res, GL_SRGB8, fb);

    glUseProgram(program.id);

    glUniform1i(program.colorTexture, 0);
    glUniform2f(program.texelStep, 1.0f/res.w, 1.0f/res.h);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, colorTexure);
    // No mipmap, so the default of GL_LINEAR_MIPMAP_LINEAR would just be black.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    glBindVertexArray(pass.emptyVao);

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fb->id);
    if (clearBackBuffer)
        glClear(GL_COLOR_BUFFER_BIT);

    glDisable(GL_DEPTH_TEST);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glEnable(GL_DEPTH_TEST);

    glBindTexture(GL_TEXTURE_2D, 0);
    glBindVertexArray(0);
    glUseProgram(0);
}

//}

extern b32
renderer_init(Memory_Arena* storage, Memory_Arena* workspace)
{
    *workspace = sub_allocate(*storage, Kilobytes(4), 16, "Rendering Workspace");

    OpenGL_Renderer* renderer = push_new(*workspace, OpenGL_Renderer);

    Shader_Catalog& catalog = renderer->shaderCatalog;

    if (!load_shader("assets/shaders/basic.vs",           GL_VERTEX_SHADER,   &catalog.basicVertexShader))          return false;
    if (!load_shader("assets/shaders/basic_instanced.vs", GL_VERTEX_SHADER,   &catalog.basicInstancedVertexShader)) return false;
    if (!load_shader("assets/shaders/static_mesh.vs",     GL_VERTEX_SHADER,   &catalog.staticMeshVertexShader))     return false;
    if (!load_shader("assets/shaders/fxaa.vs",            GL_VERTEX_SHADER,   &catalog.fxaaVertexShader))           return false;

    if (!load_shader("assets/shaders/solid.fs",           GL_FRAGMENT_SHADER, &catalog.solidFramentShader))         return false;
    if (!load_shader("assets/shaders/static_mesh.fs",     GL_FRAGMENT_SHADER, &catalog.staticMeshFragmentShader))   return false;
    if (!load_shader("assets/shaders/fxaa.fs",            GL_FRAGMENT_SHADER, &catalog.fxaaFragmentShader))         return false;

    if (!load_lines_program(catalog, &renderer->linesProgram))            return false;
    if (!load_cubes_program(catalog, &renderer->cubesProgram))            return false;
    if (!load_static_mesh_program(catalog, &renderer->staticMeshProgram)) return false;
    if (!load_fxaa_program(catalog, &renderer->fxaaProgram))              return false;

    if (!load_debug_cube_buffers(&renderer->debugCubeVertexBuffer, &renderer->debugCubeIndexBuffer)) return false;
    if (!load_imgui(&renderer->imgui)) return false;

    init_fxaa_pass(renderer->fxaaProgram, renderer->res, &renderer->aaState.fxaaPass);

    // NOTE(blake): you _need_ to specify the blend equation/func.
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glEnable(GL_FRAMEBUFFER_SRGB);

    glCullFace(GL_BACK);

    return true;
}

static void
renderer_begin_frame_internal(Memory_Arena* workspace, void* commands, u32 count, GLbitfield toClear)
{
    OpenGL_Renderer* renderer = (OpenGL_Renderer*)workspace->start;

    Render_Command_Header* header = (Render_Command_Header*)commands;
    for (u32 i = 0; i < count; i++, header = next_header(header, header->size)) {
        switch (header->type) {
        case RenderCommand_Set_Clear_Color: {
            Set_Clear_Color* cmd = render_command_after<Set_Clear_Color>(header);
            glClearColor(cmd->r, cmd->g, cmd->b, cmd->a);
            break;
        }
        case RenderCommand_Set_Viewport: {
            Set_Viewport* cmd = render_command_after<Set_Viewport>(header);
            glViewport(cmd->x, cmd->y, cmd->w, cmd->h);
            log_debug("Set Viewport: %u %u\n", cmd->w, cmd->h);
            break;
        }
        case RenderCommand_Set_View_Matrix: {
            Set_View_Matrix* cmd = render_command_after<Set_View_Matrix>(header);
            memcpy(&renderer->viewMatrix, cmd->mat4, sizeof(renderer->viewMatrix));
            break;
        }
        case RenderCommand_Set_Projection_Matrix: {
            Set_Projection_Matrix* cmd = render_command_after<Set_Projection_Matrix>(header);
            memcpy(&renderer->projectionMatrix, cmd->mat4, sizeof(renderer->projectionMatrix));
            break;
        }
        case RenderCommand_Set_AA_Technique: {

            Set_AA_Technique* cmd = render_command_after<Set_AA_Technique>(header);
            assert(cmd->technique != AA_INVALID);

            OpenGL_AA_State& aaState = renderer->aaState;

            // Nothing to do if switching to the same technique.
            if (aaState.technique == cmd->technique) break;

            // NOTE(blake): it's no big deal to keep the fxaa pass around.
            // @TheDumbThing(blake). Resizing a buffer might be faster than re-creating it, for example.
            if (aaState.msaaOn)                    free_msaa_pass(&aaState.msaaPass);
            if (aaState.technique == AA_FXAA)      free_framebuffer(&aaState.fxaaInputFbo);

            u32 sampleCount = 1;
            b32 fxaaOn      = false;

            switch (cmd->technique) {
            case AA_MSAA_2X:      sampleCount = 2;  break;
            case AA_MSAA_4X:      sampleCount = 4;  break;
            case AA_MSAA_8X:      sampleCount = 8;  break;
            case AA_MSAA_16X:     sampleCount = 16; break;
            case AA_FXAA:         fxaaOn = true;    break;
            default: break;
            }

            b32 msaaOn = (sampleCount > 1);

            // NOTE(blake): We don't need to do anything special for FXAA when it's not by itself.
            // We can draw from an MSAA pass result directly to the back buffer; however, we can't
            // read from the default back buffer, much less write back _to_ the back buffer.
            //
            if (cmd->technique == AA_FXAA) {
                create_framebuffer(renderer->res, GL_SRGB8_ALPHA8, GL_DEPTH_COMPONENT16,
                                   1, &aaState.fxaaInputFbo);
            }
            else if (msaaOn) {
                load_msaa_pass(renderer->res, sampleCount, &aaState.msaaPass);
            }

            aaState.technique = cmd->technique;
            aaState.msaaOn    = msaaOn;
            aaState.fxaaOn    = fxaaOn;

            assert(!(msaaOn && fxaaOn) && "Hybrid aa techniques no longer supported");

            break;
        }
        case RenderCommand_Resize_Buffers: {
            Resize_Buffers* resize = render_command_after<Resize_Buffers>(header);

            Game_Resolution newRes = { resize->w, resize->h };
            if (renderer->res == newRes) break;

            OpenGL_AA_State& aaState = renderer->aaState;
            assert(aaState.technique != AA_INVALID);

            renderer->res = newRes;

            // The default back buffer is resized automatically by the window manager.
            if (aaState.technique == AA_NONE)
                break;

            if (aaState.technique == AA_FXAA) {
                free_framebuffer(&aaState.fxaaInputFbo);
                create_framebuffer(newRes, GL_SRGB8_ALPHA8, GL_DEPTH_COMPONENT16,
                                   1, &aaState.fxaaInputFbo);
            }
            else if (aaState.msaaOn) {
                u32 sampleCount = aaState.msaaPass.sampleCount;

                free_msaa_pass(&aaState.msaaPass);
                load_msaa_pass(newRes, sampleCount, &aaState.msaaPass);
            }

            break;
        }
        }
    }

    glClear(toClear);
}

extern void
renderer_begin_frame(Memory_Arena* workspace, void* commands, u32 count)
{
    OpenGL_Renderer* renderer = (OpenGL_Renderer*)workspace->start;
    OpenGL_AA_State& aaState  = renderer->aaState;

    if (aaState.technique == AA_NONE) {
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        glDrawBuffer(GL_BACK);
    }
    else if (aaState.technique == AA_FXAA) {
        glDrawBuffer(GL_BACK);
        glClear(GL_COLOR_BUFFER_BIT);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, aaState.fxaaInputFbo.id);
    }
    else if (aaState.msaaOn) {
        glDrawBuffer(GL_BACK);
        glClear(GL_COLOR_BUFFER_BIT);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, aaState.msaaPass.framebuffer);
    }

    renderer_begin_frame_internal(workspace, commands, count, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

extern b32
renderer_exec(Memory_Arena* workspace, void* commands, u32 count)
{
    OpenGL_Renderer* renderer = (OpenGL_Renderer*)workspace->start;

    renderer->pointLight = nullptr;

    allocator_scope(workspace);

    Render_Command_Header* header = (Render_Command_Header*)commands;
    for (u32 i = 0; i < count; i++, header = next_header(header, header->size)) {
        switch (header->type) {
        case RenderCommand_Render_Debug_Lines: {
            Render_Debug_Lines* cmd = render_command_after<Render_Debug_Lines>(header);

            Lines_Program& program = renderer->linesProgram;
            glUseProgram(program.id);

            mat4 mvp = renderer->projectionMatrix * renderer->viewMatrix;
            glUniformMatrix4fv(program.mvp, 1, GL_FALSE, glm::value_ptr(mvp));
            glUniform3f(program.color, cmd->r, cmd->g, cmd->b);

            bind_debug_lines(cmd);
            glDrawArrays(GL_LINES, 0, cmd->vertexCount);
            glBindVertexArray(0);
            glUseProgram(0);

            break;
        }
        case RenderCommand_Render_Debug_Cubes: {
            Render_Debug_Cubes* cmd = render_command_after<Render_Debug_Cubes>(header);

            Cubes_Program& program = renderer->cubesProgram;
            glUseProgram(program.id);

            bind_debug_cubes(renderer->debugCubeVertexBuffer,
                             renderer->debugCubeIndexBuffer,
                             cmd);

            mat4 viewProjection = renderer->projectionMatrix * renderer->viewMatrix;
            glUniformMatrix4fv(program.viewProjection, 1, GL_FALSE, glm::value_ptr(viewProjection));
            glUniform3f(program.color, cmd->r, cmd->g, cmd->b);
            glUniform1f(program.scale, cmd->halfWidth);

            glDrawElementsInstanced(GL_TRIANGLES, 36, GL_UNSIGNED_BYTE, 0, cmd->count);

            glUseProgram(0);
            break;
        }
        case RenderCommand_Render_Textured_Quad: {
            break;
        }
        case RenderCommand_Render_Static_Mesh: {
            Render_Static_Mesh* cmd = render_command_after<Render_Static_Mesh>(header);

            Static_Mesh_Program& program = renderer->staticMeshProgram;
            glUseProgram(program.id);

            mat4 modelView = renderer->viewMatrix * *(mat4*)&cmd->modelMatrix;
            glUniformMatrix4fv(program.modelViewMatrix,  1, GL_FALSE, glm::value_ptr(modelView));
            glUniformMatrix4fv(program.projectionMatrix, 1, GL_FALSE, glm::value_ptr(renderer->projectionMatrix));

            mat3 normalMatrix = mat3(glm::inverseTranspose(modelView));
            glUniformMatrix3fv(program.normalMatrix, 1, GL_FALSE, glm::value_ptr(normalMatrix));

            if (renderer->pointLight) {
                const Render_Point_Light& light = *renderer->pointLight;

                v3 cSpaceLightPos = v3(renderer->viewMatrix * v4(light.x, light.y, light.z, 1));
                glUniform1i(program.lit, 1);
                glUniform3fv(program.lightPos, 1, glm::value_ptr(cSpaceLightPos));
                glUniform4f(program.lightColor, light.r, light.g, light.b, 1);
            }
            else {
                glUniform1i(program.lit, 0);
            }

            Staged_Static_Mesh* stagedMesh = stage_static_mesh(cmd);
            glBindVertexArray(stagedMesh->vao);

            for (u32 i = 0; i < stagedMesh->groupCount; i++) {
                Staged_Colored_Index_Group& group = stagedMesh->groups[i];

                glUniform1f(program.specularExp, group.specularExp);

                if (group.diffuseMap == GL_INVALID_VALUE) {
                    glUniform1i(program.solid, 1);
                    glUniform4fv(program.color, 1, glm::value_ptr(group.color));
                }
                else {
                    glUniform1i(program.solid, 0);
                    glUniform1i(program.diffuseMap, 0);

                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D, group.diffuseMap);
                }

                if (group.normalMap == GL_INVALID_VALUE) {
                    glUniform1i(program.hasNormalMap, 0);
                }
                else {
                    glUniform1i(program.hasNormalMap, 1);
                    glUniform1i(program.normalMap, 1);

                    glActiveTexture(GL_TEXTURE1);
                    glBindTexture(GL_TEXTURE_2D, group.normalMap);
                }

                // TODO: other maps.
                if (group.specularMap == GL_INVALID_VALUE) {
                    glUniform1i(program.hasSpecularMap, 0);
                }
                else {
                    glUniform1i(program.hasSpecularMap, 1);
                    glUniform1i(program.specularMap, 2);

                    glActiveTexture(GL_TEXTURE2);
                    glBindTexture(GL_TEXTURE_2D, group.specularMap);
                }

                glDrawElements(GL_TRIANGLES, group.indexCount, group.indexType, (void*)(umm)group.indexStart);
            }

            glUseProgram(0);
            glBindVertexArray(0);

            break;
        }
        case RenderCommand_Render_Point_Light: {
            Render_Point_Light* cmd = render_command_after<Render_Point_Light>(header);
            renderer->pointLight = cmd;

            break;
        }
        default:
            break;
        }
    }

    return true;
}

// NOTE(blake): this is responsible for setting GL_DRAW_FRAMEBUFFER to where
// the ui pass is supposed to be rendered to. Right now, that just means finishing
// up anti-aliasing.
//
static void
prep_for_ui_pass(OpenGL_Renderer* renderer)
{
    OpenGL_AA_State& aaState = renderer->aaState;
    assert(aaState.technique != AA_INVALID);

    if (aaState.technique == AA_NONE) {
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        glDrawBuffer(GL_BACK);
    }
    else if (aaState.technique == AA_FXAA) {
        assert(aaState.fxaaInputFbo.id != GL_INVALID_VALUE);

        // @Hack(blake) @Cleanup. I am causing the fxaa rendering function to pass 0 to
        // glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0) to make the function draw to
        // the back buffer since I am also calling glDrawBuffer(GL_BACK);
        //
        Framebuffer useBackBuffer;
        useBackBuffer.id = 0;

        glDrawBuffer(GL_BACK);

        Game_Resolution res = gGame->clientRes;
        render_fxaa_pass_to_color_fbo(renderer->fxaaProgram, aaState.fxaaPass,
                                      res, aaState.fxaaInputFbo.color,
                                      &useBackBuffer, false);
    }
    else { // MSAA only
        assert(aaState.msaaOn);

        Game_Resolution res = renderer->res;

        // Resolve directly to back buffer. This requires the back buffer to have the
        // same resolution!!

        glBindFramebuffer(GL_READ_FRAMEBUFFER, aaState.msaaPass.framebuffer);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        glDrawBuffer(GL_BACK);
        glBlitFramebuffer(0, 0, res.w, res.h, 0, 0, res.w, res.h, GL_COLOR_BUFFER_BIT, GL_NEAREST);

        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0); // just cleanup
    }
}

extern void
renderer_end_frame(Memory_Arena* ws, struct ImDrawData* drawData)
{
    OpenGL_Renderer* renderer = (OpenGL_Renderer*)ws->start;
    ImGui_Resources& imgui    = renderer->imgui;

    // Set GL_DRAW_FRAMEBUFFER to where we need to draw to based on AA state.
    prep_for_ui_pass(renderer);

    // Avoid rendering when minimized, scale coordinates for retina displays (screen coordinates != framebuffer coordinates)
    ImGuiIO& io  = ImGui::GetIO();
    int fbWidth  = (int)(drawData->DisplaySize.x * io.DisplayFramebufferScale.x);
    int fbHeight = (int)(drawData->DisplaySize.y * io.DisplayFramebufferScale.y);
    if (fbWidth <= 0 || fbHeight <= 0)
        return;

    drawData->ScaleClipRects(io.DisplayFramebufferScale);

    glEnable(GL_SCISSOR_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_FRAMEBUFFER_SRGB);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    glUseProgram(imgui.program.id);
    glUniform1i(imgui.program.texture, 0);

    v2 topLeft      = drawData->DisplayPos;
    v2 bottomRight  = (v2)drawData->DisplayPos + (v2)drawData->DisplaySize;
    mat4 projection = glm::ortho(topLeft.x, bottomRight.x, bottomRight.y, topLeft.y);
    glUniformMatrix4fv(imgui.program.projectionMatrix, 1, GL_FALSE, glm::value_ptr(projection));

    glBindVertexArray(imgui.vao);

    glBindBuffer(GL_ARRAY_BUFFER, imgui.vertexBuffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, imgui.indexBuffer);

    glVertexAttribPointer(0, 2, GL_FLOAT,         GL_FALSE, sizeof(ImDrawVert), (void*)offsetof(ImDrawVert, pos));
    glVertexAttribPointer(1, 2, GL_FLOAT,         GL_FALSE, sizeof(ImDrawVert), (void*)offsetof(ImDrawVert, uv));
    glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE,  sizeof(ImDrawVert), (void*)offsetof(ImDrawVert, col));

    for (int listI = 0; listI < drawData->CmdListsCount; listI++) {
        const ImDrawList* cmdList      = drawData->CmdLists[listI];
        const ImDrawVert* vertexBuffer = cmdList->VtxBuffer.Data;
        const ImDrawIdx*  indexBuffer  = cmdList->IdxBuffer.Data;

        glBufferData(GL_ARRAY_BUFFER, cmdList->VtxBuffer.Size * sizeof(ImDrawVert), vertexBuffer, GL_STREAM_DRAW);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, cmdList->IdxBuffer.Size * sizeof(ImDrawIdx), indexBuffer, GL_STREAM_DRAW);

        const ImDrawIdx* indexBufferOffset = 0;

        for (int cmdI = 0; cmdI < cmdList->CmdBuffer.Size; cmdI++) {
            const ImDrawCmd* cmd = &cmdList->CmdBuffer[cmdI];
            if (cmd->UserCallback) {
                cmd->UserCallback(cmdList, cmd);
            }
            else {
                GLuint texture = (GLuint)(umm)cmd->TextureId;
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, texture);

                v4 clipRect = v4(cmd->ClipRect.x - topLeft.x, cmd->ClipRect.y - topLeft.y, cmd->ClipRect.z - topLeft.x, cmd->ClipRect.w - topLeft.y);
                if (clipRect.x < fbWidth && clipRect.y < fbHeight && clipRect.z >= 0.0f && clipRect.q >= 0.0f) {
                    // NOTE(blake): from IMGUI docs:
                    // In the interest of supporting multi-viewport applications in the future (see 'viewport' branch on github),
                    // always subtract draw_data->DisplayPos from clipping bounds to convert them to your viewport space.
                    // Note that pcmd->ClipRect contains Min+Max bounds. Some graphics API may use Min+Max, other may use Min+Size (size being Max-Min)
                    glScissor((int)cmd->ClipRect.x, (int)(fbHeight - cmd->ClipRect.w),
                              (int)(cmd->ClipRect.z - cmd->ClipRect.x), (int)(cmd->ClipRect.w - cmd->ClipRect.y));

                    // NOTE(blake): from IMGUI docs:
                    // Render 'pcmd->ElemCount/3' indexed triangles.
                    // By default the indices ImDrawIdx are 16-bits, you can change them to 32-bits in imconfig.h if your engine doesn't support 16-bits indices.
                    constexpr GLenum kIndexType = sizeof(ImDrawIdx) == 2 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
                    glDrawElements(GL_TRIANGLES, cmd->ElemCount, kIndexType, indexBufferOffset);
                }
            }

            indexBufferOffset += cmd->ElemCount;
        }
    }

    glBindTexture(GL_TEXTURE_2D, 0);
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glUseProgram(0);

    glEnable(GL_FRAMEBUFFER_SRGB);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glDisable(GL_SCISSOR_TEST);
}
