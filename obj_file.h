
struct OBJ_Material_Group
{
    string32 material;
    u32 startingIndex;
};

struct OBJ_File
{
    const char* error;
    string32 mtllib;

    OBJ_Material_Group* groups;

    v3* vertices;
    v3* normals;
    v3* tangents;
    v2* uvs;

    void* indices;
    u32 indexSize;

    u32 vertexCount;
    u32 indexCount;
    u32 groupCount;
};

struct MTL_Material
{
    string32 name;

    v3 ambientColor;
    v3 diffuseColor;
    v3 specularColor;
    v3 emissiveColor;

    f32 specularExponent;
    f32 opacity;
    int illum;

    string32 ambientMap;
    string32 diffuseMap;
    string32 specularMap;
    string32 emissiveMap;
    string32 normalMap;
};

struct MTL_File
{
    const char* error;

    MTL_Material* materials;
    u32 materialCount;
};

enum Post_Process_Flags : u32
{
    PostProcess_None        = 0x0,
    PostProcess_GenNormals  = 0x1,
    PostProcess_GenTangents = PostProcess_GenNormals | 0x2,
    PostProcess_FlipUVs     = 0x4
};

static OBJ_File obj_load(buffer32 buffer, u32 processFlags = 0);
static MTL_File mtl_load(buffer32 buffer);

