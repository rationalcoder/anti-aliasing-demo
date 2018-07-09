#include <GL/gl3w.h>
#include <glm/vec3.hpp>
#include <glm/matrix.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <cstdio>

#include "demo.h"
#include "platform.h"
#include "tanks.h"
#include "mesh_loading.h"
#include "scene_printer.h"

#define STATIC_MESH_VS "demo/static_mesh.vs"
#define STATIC_MESH_FS "demo/static_mesh.fs"

#define LINES_VS "demo/lines.vs"
#define LINES_FS "demo/lines.fs"

#define TEST_MESH1 "demo/cube.obj"
#define TEST_MESH1 "demo/cube.obj"

#define BufferOffset(x) ((void*)x)
#define StackAllocate(count, type) ((type*)(alloca(sizeof(type) * count)))

struct File_View
{
    u8* data = nullptr;
    u32 size = 0;
};

static inline b32
debug_read_entire_file(const char* fileName, File_View* view)
{
    FILE* f = fopen(fileName, "rb");
    if (!f) return false;

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    rewind(f);

    void* buf = malloc(len);
    if (fread(buf, 1, len, f) != (size_t)len)
        return false;

    fclose(f);

    view->data = (u8*)buf;
    view->size = len;
    return true;
}

static inline b32
compile_shader_from_source(GLuint shader, File_View view)
{
    glShaderSource(shader, 1, (GLchar**)&view.data, (GLint*)&view.size);
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

static inline void
points_by_step(u32 n, glm::vec3 from, glm::vec3 step, glm::vec3** out)
{
    glm::vec3* cur = *out;
    glm::vec3* onePastLast = cur + n;
    for (; cur != onePastLast; cur++, from += step)
        *cur = from;

    *out = onePastLast;
}

static inline void
lines_by_step(u32 n, glm::vec3 p1, glm::vec3 p2,
              glm::vec3 step, glm::vec3** out)
{
    glm::vec3* iter = *out;
    glm::vec3* onePastLast = iter + 2*n;
    for (; iter != onePastLast; iter += 2, p1 += step, p2 += step) {
        iter[0] = p1;
        iter[1] = p2;
    }

    *out = onePastLast;
}

static inline void
lines_by_step(u32 n, glm::vec3 p1, glm::vec3 p2,
              glm::vec3 step1, glm::vec3 step2,
              glm::vec3** out)
{
    glm::vec3* iter = *out;
    glm::vec3* onePastLast = iter + 2*n;
    for (; iter != onePastLast; iter += 2, p1 += step1, p2 += step2) {
        iter[0] = p1;
        iter[1] = p2;
    }

    *out = onePastLast;
}

static b32
make_xy_plane(int xMax, int yMax, XY_Plane* plane)
{
    u32 xSpan = 2*xMax + 1;
    u32 ySpan = 2*yMax + 1;
    u32 numLines = xSpan + ySpan;
    u32 bufferSize = 2*(xSpan + ySpan) * sizeof(glm::vec3);

    glm::vec3* points = (glm::vec3*)malloc(bufferSize);
    glm::vec3* start = points;

    glm::vec3 tl {-xMax,  yMax, 0};
    glm::vec3 tr { xMax,  yMax, 0};
    glm::vec3 bl {-xMax, -yMax, 0};

    glm::vec3 xStep {1,  0, 0};
    glm::vec3 yStep {0, -1, 0};

    lines_by_step(xSpan, tl, bl, xStep, &start);
    lines_by_step(ySpan, tl, tr, yStep, &start);

    GLuint vao = GL_INVALID_VALUE;
    glGenVertexArrays(1, &vao);

    GLuint vbo = GL_INVALID_VALUE;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, bufferSize, points, GL_STATIC_DRAW);

    glBindVertexArray(vao);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, BufferOffset(0));
    glBindVertexArray(0);

    plane->vao          = vao;
    plane->vertexBuffer = vbo;
    plane->numLines     = numLines;

    free(points);
    return true;
}

static b32
load_program(const char* vsFile, const char* fsFile, GLuint* program)
{
    fprintf(stderr, "NOTE: Leaking file views on error.\n");

    File_View vsSrc;
    if (!debug_read_entire_file(vsFile, &vsSrc)) {
        fprintf(stderr, "failed to open '%s'!\n", vsFile);
        return false;
    }

    File_View fsSrc;
    if (!debug_read_entire_file(fsFile, &fsSrc)) {
        fprintf(stderr, "failed to open '%s'!\n", fsFile);
        return false;
    }

    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);

    char errorBuffer[256] = { 0 };
    if (!compile_shader_from_source(vs, vsSrc)) {
        glGetShaderInfoLog(vs, sizeof(errorBuffer), nullptr, errorBuffer);
        fprintf(stderr, "ERROR: '%s': %s\n", vsFile, errorBuffer);
        return false;
    }

    if (!compile_shader_from_source(fs, fsSrc)) {
        glGetShaderInfoLog(fs, sizeof(errorBuffer), nullptr, errorBuffer);
        fprintf(stderr, "ERROR: '%s': %s\n", fsFile, errorBuffer);
        return false;
    }

    GLuint newProgram = GL_INVALID_VALUE;

    if (!create_and_link_program(vs, fs, &newProgram)) {
        glGetProgramInfoLog(newProgram, sizeof(errorBuffer), nullptr, errorBuffer);
        fprintf(stderr, "ERROR: common program failed to link: %s\n", errorBuffer);
        return false;
    }

    free(vsSrc.data);
    free(fsSrc.data);
    *program = newProgram;
    return true;
}

static inline GLenum
to_gl_index_type(Index_Size size)
{
    switch (size) {
    case IndexSize_u8:  return GL_UNSIGNED_BYTE;
    case IndexSize_u16: return GL_UNSIGNED_SHORT;
    case IndexSize_u32: return GL_UNSIGNED_INT;
    }

    InvalidCodePath();
    return GL_UNSIGNED_INT;
}

static inline GLint
to_gl_format(Texture_Format fmt)
{
    switch (fmt) {
    case TextureFormat_RGB: return GL_RGB;
    case TextureFormat_RGBA: return GL_RGBA;
    default: assert(!"Unhandled texture format");
    }

    return GL_INVALID_VALUE;
}

static inline GLenum
to_gl_primitive(u32 flags)
{
    switch (flags) {
    case aiPrimitiveType_POINT: return GL_POINTS;
    case aiPrimitiveType_LINE: return GL_LINES;
    case aiPrimitiveType_TRIANGLE: return GL_TRIANGLES;
    default: assert(!"Meshes with more than 1 primitive type are not supported!");
    }

    return GL_TRIANGLES;
}

static inline GLuint
stage_texture(Texture_Result* result, b32 generateMipMaps = false)
{
    if (!result->bytes)
        return GL_INVALID_VALUE;

    GLuint tex = GL_INVALID_VALUE;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    GLint format = to_gl_format(result->format);
    glTexImage2D(GL_TEXTURE_2D, 0, format, result->x, result->y, 0, format, GL_UNSIGNED_BYTE, result->bytes);

    GLint minFilter = GL_LINEAR;
    if (generateMipMaps) {
        glGenerateMipmap(GL_TEXTURE_2D);
        minFilter = GL_LINEAR_MIPMAP_LINEAR;
    }

    // TODO: actually consider the texture information from assimp.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minFilter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glBindTexture(GL_TEXTURE_2D, 0);

    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        fprintf(stderr, "Failed to stage texture: %x\n", error);
        return GL_INVALID_VALUE;
    }

    return tex;
}

static inline b32
stage_mesh(const Raw_Mesh* mesh, Texture_Result* textures, Staged_Mesh* out)
{
    assert(mesh->has_indices() && mesh->has_uvs() && "Unsupported mesh");

    glGenVertexArrays(1, &out->arrayObject);
    glGenBuffers(1, &out->vertexBuffer);
    glGenBuffers(1, &out->uvBuffer);
    glGenBuffers(1, &out->indexBuffer);

    glBindVertexArray(out->arrayObject);

    // Vertices
    glBindBuffer(GL_ARRAY_BUFFER, out->vertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, (3*sizeof(f32)) * mesh->vertexCount, mesh->vertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, BufferOffset(0));

    // Normals
    if (mesh->has_normals()) {
        glGenBuffers(1, &out->normalBuffer);
        glBindBuffer(GL_ARRAY_BUFFER, out->normalBuffer);
        glBufferData(GL_ARRAY_BUFFER, (3*sizeof(f32)) * mesh->vertexCount, mesh->normals, GL_STATIC_DRAW);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, BufferOffset(0));
    }

    // UV's
    glBindBuffer(GL_ARRAY_BUFFER, out->uvBuffer);
    glBufferData(GL_ARRAY_BUFFER, (2*sizeof(f32)) * mesh->vertexCount, mesh->uvs, GL_STATIC_DRAW);
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 0, BufferOffset(0));

    // Indices
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, out->indexBuffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh->indexSize * mesh->indexCount, mesh->indices, GL_STATIC_DRAW);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    out->diffuseMap  = stage_texture(&textures[TextureMapType_Diffuse], true);
    out->normalMap   = stage_texture(&textures[TextureMapType_Normal]);
    out->specularMap = stage_texture(&textures[TextureMapType_Specular]);

    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        fprintf(stderr, "Failed to stage mesh: %x\n", error);
        return false;
    }

    out->vertexCount = mesh->vertexCount;
    out->indexCount  = mesh->indexCount;
    out->primitive   = to_gl_primitive(mesh->primitive);
    out->indexType   = to_gl_index_type(mesh->indexSize);

    return true;
}

// basic => vertices and indices
static inline b32
stage_basic_mesh(const Raw_Mesh* mesh, Staged_Mesh* out)
{
    glGenVertexArrays(1, &out->arrayObject);
    glGenBuffers(1, &out->vertexBuffer);
    glGenBuffers(1, &out->indexBuffer);

    glBindVertexArray(out->arrayObject);

    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, out->vertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, (3*sizeof(f32)) * mesh->vertexCount, mesh->vertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, BufferOffset(0));

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, out->indexBuffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh->indexSize * mesh->indexCount, mesh->indices, GL_STATIC_DRAW);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        fprintf(stderr, "Failed to stage mesh: %x\n", error);
        return false;
    }

    out->indexCount  = mesh->indexCount;
    out->vertexCount = mesh->vertexCount;
    out->primitive   = to_gl_primitive(mesh->primitive);
    out->indexType   = to_gl_index_type(mesh->indexSize);

    return true;
}


static b32
load_test_mesh(const char* fileName, const char* assetDir, Test_Mesh* mesh)
{
    int postProcess = aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_SortByPType
            /*| aiProcess_FindDegenerates*? | aiProcess_GenSmoothNormals*/;

    const aiScene* scene = aiImportFile(fileName, postProcess);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE ||
         scene->mFlags & AI_SCENE_FLAGS_VALIDATION_WARNING) {
        fprintf(stderr, "Error: %s\n", aiGetErrorString());
        return false;
    }

    //print_scene_summary(fileName, scene);

    // Skip non-triangle meshes.
    u32 subMeshCount = 0;
    for (u32 i = 0; i < scene->mNumMeshes; i++) {
        if (scene->mMeshes[i]->mPrimitiveTypes == aiPrimitiveType_TRIANGLE)
            subMeshCount++;
    }

    Raw_Mesh* meshes = new Raw_Mesh[subMeshCount];
    if (!load_static_meshes(scene, meshes)) {
        fprintf(stderr, "Failed to load '%s'\n", fileName);
        return false;
    }

    mesh->subMeshes    = new Staged_Mesh[subMeshCount];
    mesh->subMeshCount = subMeshCount;

    Texture_Result textures[TextureMapType_Num_];

    for (u32 i = 0; i < subMeshCount; i++) {
        if (!load_texture(scene, i, assetDir, aiTextureType_DIFFUSE, &textures[TextureMapType_Diffuse])) {
            fprintf(stderr, "Failed to load diffuse map!\n");
            return false;
        }

        // These two are optional.
        load_texture(scene, i, assetDir, aiTextureType_NORMALS,  &textures[TextureMapType_Normal]);
        load_texture(scene, i, assetDir, aiTextureType_SPECULAR, &textures[TextureMapType_Specular]);

        assert(stage_mesh(&meshes[i], textures, &mesh->subMeshes[i]));
    }

    free_textures(textures, TextureMapType_Num_);
    free_meshes(meshes, subMeshCount);
    delete [] meshes;
    aiReleaseImport(scene);
    return true;
}

static inline void
grab_common_locations(Common_Resources* common)
{
    Common_Locations& locations = common->shaderLocations;
    locations.linesMVP   = glGetUniformLocation(common->linesProgram, "u_mvp");
    locations.linesColor = glGetUniformLocation(common->linesProgram, "u_color");

    locations.modelMatrix      = glGetUniformLocation(common->staticMeshProgram, "u_modelMatrix");
    locations.viewMatrix       = glGetUniformLocation(common->staticMeshProgram, "u_viewMatrix");
    locations.projectionMatrix = glGetUniformLocation(common->staticMeshProgram, "u_projectionMatrix");
    locations.normalMatrix     = glGetUniformLocation(common->staticMeshProgram, "u_normalMatrix");

    locations.diffuseMap  = glGetUniformLocation(common->staticMeshProgram, "u_diffuse");
    locations.normalMap   = glGetUniformLocation(common->staticMeshProgram, "u_normal");
    locations.specularMap = glGetUniformLocation(common->staticMeshProgram, "u_specular");

    locations.hasNormalMap   = glGetUniformLocation(common->staticMeshProgram, "u_hasNormalMap");
    locations.hasSpecularMap = glGetUniformLocation(common->staticMeshProgram, "u_hasSpecularMap");

    locations.specularExp = glGetUniformLocation(common->staticMeshProgram, "u_specularExp");
    locations.lit         = glGetUniformLocation(common->staticMeshProgram, "u_lit");
    locations.lightPos    = glGetUniformLocation(common->staticMeshProgram, "u_pointLightP");
    locations.lightColor  = glGetUniformLocation(common->staticMeshProgram, "u_pointLightC");
}

// TODO
static inline b32
load_ui(UI* ui)
{
    File_View file;
    if (!debug_read_entire_file("c:/windows/fonts/arial.ttf", &file)) {
        fprintf(stderr, "Failed to load the font file!: %s\n", strerror(errno));
        return false;
    }

    stbtt_fontinfo fontInfo;
    if (!stbtt_InitFont(&fontInfo, file.data, stbtt_GetFontOffsetForIndex(file.data, 0))) {
        fprintf(stderr, "Failed to load the font!\n");
        free(file.data);
        return false;
    }

    ui->fontInfo = fontInfo;
    free(file.data);
    return true;
}

#define FXAA_ARTIFACTS 1
#define FXAA_CRAZY_ARTIFACTS 1

static b32
load_common_resources(Common_Resources* common, Game_Resolution)
{
    if (!load_program(LINES_VS, LINES_FS, &common->linesProgram))
        return false;

    if (!load_program(STATIC_MESH_VS, STATIC_MESH_FS, &common->staticMeshProgram))
        return false;

#if FXAA_ARTIFACTS && FXAA_CRAZY_ARTIFACTS
    if (!make_xy_plane(40, 40, &common->xyPlane)) {
        fprintf(stderr, "ERROR: failed to make the xy-plane.\n");
        return false;
    }
#elif FXAA_ARTIFACTS
    if (!make_xy_plane(20, 20, &common->xyPlane)) {
        fprintf(stderr, "ERROR: failed to make the xy-plane.\n");
        return false;
    }
#else
    if (!make_xy_plane(1, 1, &common->xyPlane)) {
        fprintf(stderr, "ERROR: failed to make the xy-plane.\n");
        return false;
    }
#endif

    grab_common_locations(common);

    // @leak
    constexpr u32 NUM_TEST_MESHES = 5;
    common->testMeshes    = new Test_Mesh[NUM_TEST_MESHES];
    common->testMeshCount = NUM_TEST_MESHES;

    Test_Mesh& heli = common->testMeshes[0];
    heli.modelMatrix = glm::translate(glm::vec3(0, 20, 0)) *
                       glm::rotate(glm::pi<f32>()/2, glm::vec3(0, 0, 1)) *
                       glm::scale(glm::vec3(.1, .1, .1)) *
                       glm::rotate(glm::half_pi<f32>(), glm::vec3(1, 0, 0));

    if (!load_test_mesh("demo/assets/hheli.obj", "demo/assets/", &heli)) {
        fprintf(stderr, "ERROR: failed to load the heli!\n");
        return false;
    }

    Test_Mesh& heli2 = common->testMeshes[1];
    heli2 = heli;
    heli2.modelMatrix = glm::translate(glm::vec3(-3, 0, 3)) *
                        glm::rotate(-glm::pi<f32>()/3, glm::vec3(0, 1, 0)) *
                        glm::rotate(glm::pi<f32>()/2, glm::vec3(0, 0, 1)) *
                        glm::scale(glm::vec3(.005, .005, .005)) *
                        glm::rotate(glm::half_pi<f32>(), glm::vec3(1, 0, 0));

    Test_Mesh& heli3 = common->testMeshes[2];
    heli3 = heli;
    heli3.modelMatrix = glm::translate(glm::vec3(-2.5, 0, 3)) *
                        glm::rotate(-glm::pi<f32>()/3, glm::vec3(0, 1, 0)) *
                        glm::rotate(glm::pi<f32>()/2, glm::vec3(0, 0, 1)) *
                        glm::scale(glm::vec3(.005, .005, .005)) *
                        glm::rotate(glm::half_pi<f32>(), glm::vec3(1, 0, 0));

    Test_Mesh& jeep1 = common->testMeshes[3];
    jeep1.modelMatrix = glm::scale(glm::vec3(.01, .01, .01)) * glm::rotate(glm::half_pi<f32>(), glm::vec3(1, 0, 0));
    jeep1.lit = false;

    if (!load_test_mesh("demo/assets/jeep.obj", "demo/assets/", &jeep1)) {
        fprintf(stderr, "ERROR: failed to load the jeep!\n");
        return false;
    }

    Test_Mesh& jeep2 = common->testMeshes[4];
    jeep2 = jeep1;
    jeep2.modelMatrix = glm::translate(glm::vec3(0, 6, 0)) *
                        //glm::rotate(-glm::half_pi<f32>(), glm::vec3(0, 0, 1)) *
                        glm::scale(glm::vec3(.01, .01, .01)) *
                        glm::rotate(glm::half_pi<f32>(), glm::vec3(1, 0, 0));

#if 0
    Test_Mesh& hammer = common->testMeshes[/*TODO*/];
    box.modelMatrix = glm::translate(glm::vec3(0, 0, 4));

    if (!load_test_mesh("demo/assets/hammer/IronFootAxeObj.obj", "demo/assets/hammer/", &hammer)) {
        fprintf(stderr, "ERROR: failed to load the hammer!\n");
        return false;
    }
#endif

    common->pointLight.position = glm::vec4(0, -5, 5, 1);
    common->pointLight.color    = glm::vec4(1, 1, 1, 1);

    Raw_Mesh cube;
    make_debug_cube(&cube, .2f);

    if (!stage_basic_mesh(&cube, &common->debugCube)) {
        fprintf(stderr, "ERROR: failed to make the debug cube!\n");
        free_mesh(&cube);
        return false;
    }

    free_mesh(&cube);

    if (!load_ui(&common->ui))
        return false;

    return true;
}

static inline void
grab_fxaa_locations(FXAA_Pass* pass)
{
    FXAA_Locations& locations = pass->shaderLocations;
    GLuint program            = pass->program;

    locations.on            = glGetUniformLocation(program, "u_fxaaOn");
    locations.texelStep     = glGetUniformLocation(program, "u_texelStep");
    locations.showEdges     = glGetUniformLocation(program, "u_showEdges");
    locations.lumaThreshold = glGetUniformLocation(program, "u_lumaThreshold");
    locations.mulReduce     = glGetUniformLocation(program, "u_mulReduce");
    locations.minReduce     = glGetUniformLocation(program, "u_minReduce");
    locations.maxSpan       = glGetUniformLocation(program, "u_maxSpan");
    locations.tex           = glGetUniformLocation(program, "u_colorTexture");
}

static inline b32
load_fxaa_pass(FXAA_Pass* pass, Game_Resolution res)
{
    if (!load_program("demo/fxaa.vs", "demo/fxaa.fs", &pass->program))
        return false;

    grab_fxaa_locations(pass);

    // Color buffer
    glGenTextures(1, &pass->tex);
    glBindTexture(GL_TEXTURE_2D, pass->tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, res.w, res.h, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Depth buffer
    glGenRenderbuffers(1, &pass->depthBuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, pass->depthBuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT32F, res.w, res.h);

    // Framebuffer
    glGenFramebuffers(1, &pass->fb);
    glBindFramebuffer(GL_FRAMEBUFFER, pass->fb);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pass->tex, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, pass->depthBuffer);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        fprintf(stderr, "Framebuffer incomplete: %x!\n", status);
        return false;
    }

    glBindTexture(GL_TEXTURE_2D, 0);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);

    // Empty vao cuz OpenGL.
    glGenVertexArrays(1, &pass->emptyVao);
    glBindVertexArray(pass->emptyVao);
    glBindVertexArray(0);

    FXAA_Locations& locations = pass->shaderLocations;
    glUseProgram(pass->program);
    glUniform1i(locations.tex, 0);
    glUniform2f(locations.texelStep, 1.0f / (f32)gGame->closestRes.w, 1.0f / (f32)gGame->closestRes.h);
    glUniform1i(locations.showEdges, 0);
    glUniform1i(locations.on, 1);

    // NOTE: these are _crazy_ inputs to FXAA.
    // Typically, lumaThresh = .5, mulReduce = 1/8, minReduce = 1/256, maxSpan = 5
    // These are set the way they are to show the artifacts introduced in order to
    // get reasonable anti-aliasing on the muffler thing in the jeeps.
    glUniform1f(locations.lumaThreshold, 0.01f);
    glUniform1f(locations.mulReduce, 1.0f / 64.0f);
    glUniform1f(locations.minReduce, 1.0f / 256.0f);
    glUniform1f(locations.maxSpan, 11.0f);
    glUseProgram(0);

    return true;
}

static inline b32
resize_fxaa(FXAA_Pass* pass, Game_Resolution res)
{
    glBindTexture(GL_TEXTURE_2D, pass->tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, res.w, res.h, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);

    glBindRenderbuffer(GL_RENDERBUFFER, pass->depthBuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT32F, res.w, res.h);

    glBindTexture(GL_TEXTURE_2D, 0);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        fprintf(stderr, "Framebuffer incomplete on resize!\n");
        return false;
    }

    return true;
}

static inline b32
load_msaa_pass(MSAA_Pass* pass, u32 sampleCount, Game_Resolution res)
{
    glGenTextures(1, &pass->tex);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, pass->tex);
    // NOTE: if textures and render buffers are mixed, fixedsamplelocations must be GL_TRUE!
    glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, sampleCount, GL_RGBA8, res.w, res.h, GL_TRUE);

    glGenRenderbuffers(1, &pass->depthBuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, pass->depthBuffer);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, sampleCount, GL_DEPTH_COMPONENT32F, res.w, res.h);

    glGenFramebuffers(1, &pass->fb);
    glBindFramebuffer(GL_FRAMEBUFFER, pass->fb);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, pass->tex, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, pass->depthBuffer);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        fprintf(stderr, "Framebuffer incomplete: %x!\n", status);
        return false;
    }

    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, 0);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);

    pass->sampleCount = sampleCount;
    return true;
}

static inline b32
resize_msaa(MSAA_Pass* pass, Game_Resolution res)
{
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, pass->tex);
    glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, pass->sampleCount, GL_RGBA8, res.w, res.h, GL_TRUE);

    glBindRenderbuffer(GL_RENDERBUFFER, pass->depthBuffer);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, pass->sampleCount, GL_DEPTH_COMPONENT, res.w, res.h);

    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, 0);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        fprintf(stderr, "Framebuffer incomplete on resize!\n");
        return false;
    }

    return true;
}

extern b32
demo_load(Demo_Scene* scene, Game_Resolution clientRes, Game_Resolution closestRes)
{
    glEnable(GL_BLEND);
    glEnable(GL_MULTISAMPLE);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(.2f, 0, .7f, 1);
    //glClearColor(0, 0, 0, 1);

    if (!load_common_resources(&scene->common, closestRes))
        return false;

    assert(scene->currentTechnique == AAT_MSAA_4X);
    load_msaa_pass(&scene->msaaPass, 4, closestRes);

    scene->common.projection = glm::perspective(glm::radians(70.0f), 16/9.0f, .1f, 1000.0f);
    scene->common.camera.look_at(glm::vec3(-3.409268, -0.165273, 3.565136), glm::vec3(-2.73962, .48274, 3.205046));

    glViewport(0, 0, clientRes.w, clientRes.h);

    return true;
}

extern void
demo_resize(Demo_Scene* scene, Game_Resolution clientRes, Game_Resolution closestRes)
{
    if (closestRes != gGame->closestRes) {
        switch (scene->currentTechnique) {
        case AAT_FXAA:
            resize_fxaa(&scene->fxaaPass, closestRes);
            break;
        case AAT_MSAA_4X:
        case AAT_MSAA_8X:
        case AAT_MSAA_16X:
            resize_msaa(&scene->msaaPass, closestRes);
            break;
        case AAT_None: break;
        default:
            InvalidCodePath();
            break;
        }
    }

    // printf("Viewport: %u %u\n", clientRes.w, clientRes.h);
    glViewport(0, 0, clientRes.w, clientRes.h);
}

static inline void
free_fxaa_pass(FXAA_Pass* pass)
{
    glDeleteProgram(pass->program);
    glDeleteRenderbuffers(1, &pass->depthBuffer);
    glDeleteTextures(1, &pass->tex);
    glDeleteFramebuffers(1, &pass->fb);

    pass->depthBuffer = GL_INVALID_VALUE;
    pass->tex         = GL_INVALID_VALUE;
    pass->fb          = GL_INVALID_VALUE;
}

static inline void
free_msaa_pass(MSAA_Pass* pass)
{
    glDeleteRenderbuffers(1, &pass->depthBuffer);
    glDeleteTextures(1, &pass->tex);
    glDeleteFramebuffers(1, &pass->fb);

    pass->depthBuffer = GL_INVALID_VALUE;
    pass->tex         = GL_INVALID_VALUE;
    pass->fb          = GL_INVALID_VALUE;
}

extern void
demo_next_technique(Demo_Scene* scene)
{
    switch (scene->currentTechnique) {
    case AAT_MSAA_4X:
        free_msaa_pass(&scene->msaaPass);
        load_msaa_pass(&scene->msaaPass, 8, gGame->closestRes);
        scene->currentTechnique = AAT_MSAA_8X;
        printf("MSAA 8X\n");
        break;
    case AAT_MSAA_8X:
        free_msaa_pass(&scene->msaaPass);
        load_msaa_pass(&scene->msaaPass, 16, gGame->closestRes);
        scene->currentTechnique = AAT_MSAA_16X;
        printf("MSAA 16X\n");
        break;
    case AAT_MSAA_16X:
        free_msaa_pass(&scene->msaaPass);
        load_fxaa_pass(&scene->fxaaPass, gGame->closestRes);
        scene->currentTechnique = AAT_FXAA;
        printf("FXAA\n");
        break;
    case AAT_FXAA:
        free_fxaa_pass(&scene->fxaaPass);
        scene->currentTechnique = AAT_None;
        printf("NO AA\n");
        break;
    case AAT_None:
        free_fxaa_pass(&scene->fxaaPass);
        load_msaa_pass(&scene->msaaPass, 4, gGame->closestRes);
        scene->currentTechnique = AAT_MSAA_4X;
        printf("MSAA 4X\n");
        break;
    default:
        InvalidCodePath();
        break;
    }
}

static inline f32
map_bilateral(s32 val, u32 range)
{
    return val/(range/2.0f);
}

extern void
demo_update(Demo_Scene* scene, Game_Input* input, f32 dt)
{
    Game_Keyboard& kb = input->keyboard;

    if (kb.pressed(GK_1))
        demo_next_technique(scene);

    if (kb.pressed(GK_ESCAPE)) {
        gGame->shouldQuit = true;
        return;
    }

    Camera& camera = scene->common.camera;
    if (kb.pressed(GK_2)) {
        printf("Position: (%f, %f, %f), Direction: (%f, %f, %f)\n",
               camera.position.x, camera.position.y, camera.position.z,
               camera.direction.x, camera.direction.y, camera.direction.z);
    }

    f32 camSpeed = dt;

    if (kb.down(GK_W)) camera.forward_by(camSpeed);
    if (kb.down(GK_S)) camera.forward_by(-camSpeed);
    if (kb.down(GK_A)) camera.right_by(-camSpeed);
    if (kb.down(GK_D)) camera.right_by(camSpeed);
    if (kb.down(GK_E)) camera.up_by(camSpeed);

    Game_Resolution clientRes = gGame->clientRes;
    Game_Mouse_State& temp = input->mouse.cur;
    if (temp.dragging) {
        if (temp.xDrag) {
            f32 xDrag = map_bilateral(temp.xDrag, clientRes.w);
            camera.yaw_by(2*-xDrag);
        }

        if (temp.yDrag) {
            f32 yDrag = map_bilateral(temp.yDrag, clientRes.h);
            camera.pitch_by(2*yDrag);
        }
    }
}

static void
msaa_begin_render(MSAA_Pass* pass)
{
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, pass->fb);
}

static void
msaa_end_render(MSAA_Pass* pass)
{
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, pass->fb);
    glDrawBuffer(GL_BACK);

    Game_Resolution& fbRes = gGame->closestRes;
    glBlitFramebuffer(0, 0, fbRes.w, fbRes.h, 0, 0, fbRes.w, fbRes.h, GL_COLOR_BUFFER_BIT, GL_NEAREST);
}

static void
fxaa_begin_render(FXAA_Pass* pass)
{
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, pass->fb);
    glViewport(0, 0, gGame->closestRes.w, gGame->closestRes.h);
}

static void
fxaa_end_render(FXAA_Pass* pass)
{
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glDrawBuffer(GL_BACK);

    glUseProgram(pass->program);
    glDisable(GL_DEPTH_TEST);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, pass->tex);

    FXAA_Locations& locations = pass->shaderLocations;
    glUniform2f(locations.texelStep, 1.0f / (f32)gGame->closestRes.w, 1.0f / (f32)gGame->closestRes.h);

    glBindVertexArray(pass->emptyVao);
    glViewport(0, 0, gGame->clientRes.w, gGame->clientRes.h);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
    glUseProgram(0);

    glEnable(GL_DEPTH_TEST);
}

extern void
demo_render(Demo_Scene* scene)
{
    if (scene->currentTechnique == AAT_FXAA)
        fxaa_begin_render(&scene->fxaaPass);
    else if (scene->currentTechnique != AAT_None)
        msaa_begin_render(&scene->msaaPass);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    Common_Resources& common = scene->common;
    Common_Locations& locations = common.shaderLocations;

    // Test meshes

    glUseProgram(common.staticMeshProgram);

    glm::mat4 viewMatrix = common.camera.view_matrix();
    glm::vec3 cSpaceLightPos  = glm::vec3(viewMatrix * common.pointLight.position);

    glUniform3fv(locations.lightPos,   1, glm::value_ptr(cSpaceLightPos));
    glUniform4fv(locations.lightColor, 1, glm::value_ptr(common.pointLight.color));

    glUniform1i(locations.diffuseMap,  TextureMapType_Diffuse);
    glUniform1i(locations.normalMap,   TextureMapType_Normal);
    glUniform1i(locations.specularMap, TextureMapType_Specular);

    Test_Mesh* meshes = common.testMeshes;
    for (u32 testIdx = 0; testIdx < common.testMeshCount; testIdx++) {
        Test_Mesh& testMesh = meshes[testIdx];
        for (u32 subIdx = 0; subIdx < testMesh.subMeshCount; subIdx++) {
            Staged_Mesh& stagedMesh = testMesh.subMeshes[subIdx];

            glUniformMatrix4fv(locations.modelMatrix,      1, GL_FALSE, glm::value_ptr(testMesh.modelMatrix));
            glUniformMatrix4fv(locations.viewMatrix,       1, GL_FALSE, glm::value_ptr(viewMatrix));
            glUniformMatrix4fv(locations.projectionMatrix, 1, GL_FALSE, glm::value_ptr(common.projection));
            glUniform1i(locations.lit, testMesh.lit);

            glm::mat4 mv = viewMatrix * testMesh.modelMatrix;
            glm::mat3 normalMatrix = glm::mat3(glm::inverseTranspose(mv));
            glUniformMatrix3fv(locations.normalMatrix, 1, GL_FALSE, glm::value_ptr(normalMatrix));

            glActiveTexture(GL_TEXTURE0 + TextureMapType_Diffuse);
            glBindTexture(GL_TEXTURE_2D, stagedMesh.diffuseMap);

            // NOTE: Neither of these are correcly implemented in the current shader, but whatever. I had high hopes.
            if (stagedMesh.has_normal_map()) {
                glUniform1i(locations.hasNormalMap, 1);
                glActiveTexture(GL_TEXTURE0 + TextureMapType_Normal);
                glBindTexture(GL_TEXTURE_2D, stagedMesh.normalMap);
            }

            if (stagedMesh.has_specular_map()) {
                glUniform1i(locations.hasSpecularMap, 1);
                glActiveTexture(GL_TEXTURE0 + TextureMapType_Specular);
                glBindTexture(GL_TEXTURE_2D, stagedMesh.specularMap);
            }

            glBindVertexArray(stagedMesh.arrayObject);
            glDrawElements(stagedMesh.primitive, stagedMesh.indexCount, stagedMesh.indexType, BufferOffset(0));
        }
    }

    // Background stuff

    // Point light
    glUseProgram(common.linesProgram);

    Point_Light& light = common.pointLight;
    Staged_Mesh& debugCube = common.debugCube;

    glBindVertexArray(debugCube.arrayObject);

    glm::mat4 mvp = common.projection * viewMatrix * glm::translate(glm::vec3(light.position));
    glUniformMatrix4fv(locations.linesMVP, 1, GL_FALSE, glm::value_ptr(mvp));
    glUniform4fv(locations.linesColor, 1, glm::value_ptr(light.color));

    glDrawElements(debugCube.primitive, debugCube.indexCount, debugCube.indexType, BufferOffset(0));


    // XY grid
    glUniform4f(locations.linesColor, 1, 1, 1, .1f);

    glBindVertexArray(common.xyPlane.vao);

    mvp = common.projection * viewMatrix;

    glUniformMatrix4fv(locations.linesMVP, 1, GL_FALSE, glm::value_ptr(mvp));
    glDrawArrays(GL_LINES, 0, common.xyPlane.numLines*2);

    glBindVertexArray(0);

    if (scene->currentTechnique == AAT_FXAA) {
        fxaa_end_render(&scene->fxaaPass);
        // TODO: UI
    }
    else if (scene->currentTechnique != AAT_None) {
        msaa_end_render(&scene->msaaPass);
    }
}

extern void
demo_free(Demo_Scene* scene)
{
    // @leak
}
