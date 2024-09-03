#ifndef LIGHT_H
#define LIGHT_H

#include <glm/glm.hpp>

class Light
{
public:

    glm::vec3 colour, position;
    float intensity;

    Light() {}

    Light (glm::vec3 position, glm::vec3 colour, float intensity)
        : position(position)
        , colour(colour)
        , intensity(intensity)
    {}

};

#endif