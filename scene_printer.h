#ifndef SCENE_PRINTER_H
#define SCENE_PRINTER_H

#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
//#include <assimp/cexport.h>

inline const char* anim_behavior_to_string(aiAnimBehaviour behavior)
{
    switch (behavior) {
    case aiAnimBehaviour_CONSTANT: return "Constant";
    case aiAnimBehaviour_LINEAR:   return "Linear";
    case aiAnimBehaviour_REPEAT:   return "Repeat";
    case aiAnimBehaviour_DEFAULT:  return "Default";
    default: break;
    }
    return "Uknown";
}

void ifprintf(FILE* file, uint32_t indent, const char* fmt, ...);
void iprintf(uint32_t indent, const char* fmt, ...);
void print_section_heading(const char* name);

void print_mesh_info(const aiScene* scene);

template <typename T_>
const char* ai_to_string(T_);

void print_material_info(const aiScene* scene);
void print_light_info(const aiScene* scene);
void print_camera_info(const aiScene* scene);
void print_animation_info(const aiScene* scene);
void print_node_info(const aiNode* node, uint32_t indent = 0);
void print_node_summary(const aiScene* scene);
void print_scene_summary(const char* fileName, const aiScene* scene);

#endif // SCENE_PRINTER_H
