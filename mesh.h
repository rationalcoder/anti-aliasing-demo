
enum Index_Size
{
    IndexSize_u8  = sizeof(u8),
    IndexSize_u16 = sizeof(u16),
    IndexSize_u32 = sizeof(u32),
};

enum Primitive
{
    Primitive_Points,
    Primitive_Lines,
    Primitive_Triangles,
    Primitive_Num_,
};

enum Texture_Format
{
    TextureFormat_Grey,
    TextureFormat_GreyAlpha,
    TextureFormat_RGB,
    TextureFormat_RGBA,
    TextureFormat_Num_
};

enum Texture_Type
{
    TextureType_Diffuse,
    TextureType_Normal,
    TextureType_Specular,
    TextureType_Num_,
};

struct Texture
{
    void* data = nullptr;
    Texture_Format format = TextureFormat_Num_;
    s32 x = 0;
    s32 y = 0;
};

struct Colored_Index_Group
{
    Texture diffuseMap;
    Texture normalMap;
    Texture specularMap;
    Texture emissiveMap;

    f32 specularExp;

    v3 color; // used if texture is empty
    u32 start;
    u32 count;

    b32 has_diffuse_map()  const { return !!diffuseMap.data;  }
    b32 has_normal_map()   const { return !!normalMap.data;   }
    b32 has_specular_map() const { return !!specularMap.data; }
    b32 has_emissive_map() const { return !!emissiveMap.data; }
};

struct Material
{
    Colored_Index_Group* coloredIndexGroups = nullptr;
    u32                  coloredGroupCount  = 0;
};

struct Static_Mesh
{
    string32 tag;

    f32*  vertices = nullptr;
    f32*  normals  = nullptr;
    f32*  tangents = nullptr;
    f32*  uvs      = nullptr;
    void* indices  = nullptr;

    Material* material = nullptr;

    u32 vertexCount = 0;
    u32 indexCount  = 0;

    Index_Size indexSize = IndexSize_u16;
    Primitive primitive  = Primitive_Triangles;

    b32 has_material() const { return material != nullptr; }
    b32 has_uvs()      const { return uvs      != nullptr; }
    b32 has_indices()  const { return indices  != nullptr; }
    b32 has_normals()  const { return normals  != nullptr; }
    b32 has_tangents() const { return tangents != nullptr; }
};

