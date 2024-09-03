#ifndef CAMERA_H
#define CAMERA_H

#include <glm/glm.hpp>
#include "utils.h"
#include "shader.h"

class Camera
{
public:

    float focalLength = 1.0;
    float distance = 6.0;
    float theta = 0.0;
    float phi = PI / 2.0;
    glm::vec3 lookat = glm::vec3(0.0, 0.0, 0.0);
    glm::vec3 lookfrom;

    struct Viewport {
        float width, height = 0.5;
        glm::vec3 pixelDW, pixelDH, origin;
    } viewport;

    Camera() {}

    Camera(glm::ivec2 windowDimensions, float theta, float phi, float distance, float focalLength)
        : theta(theta), phi(phi), distance(distance), focalLength(focalLength)
    {
        updateDimensions(windowDimensions);
    }

    void onUpdate()
    {
        // Lookfrom from spherical coordinates
        lookfrom = lookat + glm::vec3(
            distance*cos(theta)*sin(phi),
            distance*cos(phi),
            distance*sin(theta)*sin(phi)
        );

        // Camera frame basis vectors
        w = glm::normalize(lookfrom - lookat);
        u = glm::normalize(glm::cross(upVector, w));
        v = glm::cross(w, u);

        // Vectors across the horizontal and vertical viewport edges
        glm::vec3 viewportHorizontal = u*viewport.width;
        glm::vec3 viewportVertical   = v*viewport.height;
        
        // Horizontal and vertical delta vectors from pixel to pixel
        viewport.pixelDW = viewportHorizontal / (float)(cachedWindowDimensions.x);
        viewport.pixelDH = viewportVertical   / (float)(cachedWindowDimensions.y);
        
        // Location of the center of the upper left pixel
        glm::vec3 viewportTopLeft = lookfrom - (w*focalLength) - (viewportHorizontal / 2.0f) - (viewportVertical / 2.0f);
        viewport.origin = viewportTopLeft + 0.5f*(viewport.pixelDW + viewport.pixelDH);
    }
    
    void updateDimensions(glm::ivec2 windowDimensions)
    {
        cachedWindowDimensions = windowDimensions;
        viewport.width = viewport.height * (windowDimensions.x / (float)(windowDimensions.y));
        onUpdate();
    }

    void mouseDragCallback(glm::vec2 dpos)
    {
        float dtheta = (dpos.x / cachedWindowDimensions.x) * (2 * PI) * 0.8f;
        float dphi = (dpos.y / cachedWindowDimensions.y) * (2 * PI) * 0.8f;

        // Update camera position
        theta += dtheta;
        if (theta <= 0.0f) theta = 2*PI;
        else if (theta > 2*PI) theta = 0.0f;

        phi -= dphi;
        if (phi < 0.1f) phi = 0.1f;       // Shouldn't get to `phi = 0`
        else if (phi > PI) phi = PI;

        onUpdate();
    }

    void mouseScrollCallback(float yOffset)
    {
        if (yOffset == 0.0f) return;
        distance -= yOffset * 0.1;
        if (distance < 0.1) distance = 0.1;
        onUpdate();
    }

private:

    glm::ivec2 cachedWindowDimensions = glm::ivec2(1, 1);
    
    glm::vec3 upVector = glm::vec3(0.0, 1.0, 0.0);
    glm::vec3 u, v, w;  // Camera frame basis vectors

};

#endif