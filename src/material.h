#ifndef MATERIAL_H
#define MATERIAL_H

#include <glm/glm.hpp>

class Material
{
public:

    float roughness, metallic;
    glm::vec3 albedo, F0;

    Material() {}

    Material(float roughness, float metallic, glm::vec3 albedo)
        : roughness(roughness)
        , metallic(metallic)
        , albedo(albedo)
    { setF0(); }

    void setF0()
    {
        F0 = glm::mix(glm::vec3(0.04), albedo, metallic);
    }

};

#endif