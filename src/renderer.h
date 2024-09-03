#ifndef RENDERER_H
#define RENDERER_H

#define DEBUG_VEC3F(V) ImGui::Text(#V ": (%.4f, %.4f, %.4f)", V.x, V.y, V.z);
#define DEBUG_VEC2F(V) ImGui::Text(#V ": (%.4f, %.4f)", V.x, V.y, V.z);
#define DEBUG_VEC2I(V) ImGui::Text(#V ": (%d, %d)", V.x, V.y);

#include <stdlib.h>
#include <chrono>
#include <glm/glm.hpp>
#include <imgui/imgui.h>
#include "utils.h"
#include "shader.h"
#include "camera.h"
#include "material.h"
#include "light.h"
#include "frameInterpolator.h"

class Renderer
{
public:

    Renderer () {}

    Renderer(glm::ivec2 windowDimensions)
    {
        shader = Shader("./src/shaders/quad.vert", "./src/shaders/main.frag");
        camera = Camera(windowDimensions, 5.4, 1.3, 18.0, 3.0);
        mat = Material(0.5, 0.0, glm::vec3(0.4, 0.2, 0.0));
        light = Light(glm::vec3(1.0), glm::vec3(1.0), 5.0);
        setResolution(windowDimensions);
    }

    void onUpdate()
    {
        renderedFrameCount = 0;
        skipAA = 2;  // Skip anti aliasing for the next 2 frames
    }
    
    void renderScene(int prevTextureUnit)
    {
        static float currFrameTime = 0.0f, lastFrameTime = std::chrono::duration<float>(std::chrono::steady_clock::now().time_since_epoch()).count();

        // Start time
        currFrameTime = std::chrono::duration<float>(std::chrono::steady_clock::now().time_since_epoch()).count();
        u_time += currFrameTime - lastFrameTime;
        lastFrameTime = currFrameTime;

        // Set uniforms
        setRenderingUniforms(prevTextureUnit);
        setFractalUniforms();
        setWorldUniforms();
        setCameraUniforms();
        setMaterialUniforms();
        setLightUniforms();

        // Render scene
        shader.use();

        renderedFrameCount++;
    }

    void setResolution(glm::ivec2 newResolution)
    {
        resolution = newResolution;
        camera.updateDimensions(newResolution);
        onUpdate();
    }


    // * Uniform Setters

    void setRenderingUniforms(GLint prevTextureUnit)
    {
        doTemporalAntiAliasing = skipAA ? --skipAA > 1 : doTAA;
        shader.setBool("test", test);
        shader.setBool("doPixelSampling", doPixelSampling);
        shader.setBool("doGammaCorrection", doGammaCorrection);
        shader.setBool("doTemporalAntiAliasing", doTemporalAntiAliasing);

        shader.setInt("renderedFrameCount", renderedFrameCount);
        shader.setInt("samplingMethod", samplingMethod);
        shader.setInt("samplesPerPixel", samplesPerPixel);
        shader.setInt("prevFrameTexture", prevTextureUnit);

        shader.setVec2i("resolution", resolution);
    }

    void setFractalUniforms()
    {
        shader.setInt("maxIterations", maxIterations);
        shader.setVec4f("c", c);
        shader.setFloat("w", w);
        shader.setFloat("escapeThreshold", escapeThreshold);
        shader.setFloat("boundingRadius2", boundingRadius*boundingRadius);
        shader.setFloat("epsilon", epsilon);
    }

    void setWorldUniforms()
    {
        shader.setVec3f("backgroundColour", backgroundColour);
        shader.setFloat("u_time", u_time);
    }

    void setCameraUniforms()
    {
        shader.setFloat("cameraDistance", camera.distance);
        shader.setVec3f("lookfrom", camera.lookfrom);
        shader.setVec3f("pixelDW", camera.viewport.pixelDW);
        shader.setVec3f("pixelDH", camera.viewport.pixelDH);
        shader.setVec3f("viewportOrigin", camera.viewport.origin);
    }

    void setMaterialUniforms()
    {
        shader.setFloat("roughness", mat.roughness);
        shader.setFloat("metallic", mat.metallic);
        shader.setVec3f("albedo", mat.albedo);
        shader.setVec3f("F0", mat.F0);
    }

    void setLightUniforms()
    {
        shader.setVec3f("lightPos", light.position);
        shader.setVec3f("lightColour", light.colour);
        shader.setFloat("lightIntensity", light.intensity);
    }

    // * GUI

    bool DragFloatKeyframe(FrameInterpolator *frameInterpolator, bool *update, const char *label, float *target, float speed, float min, float max, const char *format = "%.3f")
    {   
        // Initial value setting button
        char iButtonLabel[64] = "-##i";
        strcat(iButtonLabel, label);
        if (frameInterpolator->isInitialValueActive(target))
        {
            // If initial value is active, then label button with it
            float iVal = frameInterpolator->getInitialValue(target);
            sprintf(iButtonLabel, "%.1f##i", iVal);
        }
        if (ImGui::Button(iButtonLabel))
            frameInterpolator->setInitial(target, *target);
        ImGui::SameLine();
        
        // Final value setting button
        char fButtonLabel[64] = "-##f";
        strcat(fButtonLabel, label);
        if (frameInterpolator->isFinalValueActive(target))
        {
            // If final value is active, then label button with it
            float fVal = frameInterpolator->getFinalValue(target);
            sprintf(fButtonLabel, "%.1f##f", fVal);
        }
        if (ImGui::Button(fButtonLabel))
            frameInterpolator->setFinal(target, *target);
        ImGui::SameLine();
        
        // Slider
        bool didChange = ImGui::DragFloat(label, target, speed, min, max, format);
        *update |= didChange;
        return didChange;
    }

    void menu(FrameInterpolator *frameInterpolator)
    {
        #define SECTION(TITLE, MENU) if (ImGui::CollapsingHeader(TITLE)) MENU(frameInterpolator)

        SECTION("Data", dataGui);
        SECTION("Rendering", renderingMenu);
        SECTION("Fractal", fractalMenu);
        SECTION("World", worldMenu);
        SECTION("Material", materialMenu);
        SECTION("Light", lightMenu);
        SECTION("Camera", cameraMenu);
    }

    void dataGui(FrameInterpolator *frameInterpolator)
    {
        ImGui::Text("%.4f FPS", ImGui::GetIO().Framerate);
        ImGui::Text("%d Frames sampled", renderedFrameCount);
        ImGui::Text("u_time: %.6f", u_time);
        DEBUG_VEC2I(resolution);
        
        if (frameInterpolator->isActive())
        {
            ImGui::Text("Frames Rendered: %d/%d",
                frameInterpolator->getInterpolationValue() - frameInterpolator->getMinInterpolationValue() - 1,
                frameInterpolator->getMaxInterpolationValue()
            );
        }
    }

    void renderingMenu(FrameInterpolator *frameInterpolator)
    {
        bool updated = false;

        updated |= ImGui::Checkbox("Test", (bool*)&(test));
        updated |= ImGui::Checkbox("Gamma Correction", (bool*)&(doGammaCorrection));
        updated |= ImGui::Checkbox("Temporal Anti-Aliasing", &(doTAA));
        
        if (doTAA)
        {
            // We want this on when doing temporal anti aliasing
            doPixelSampling = true;
        }
        else
        {
            updated |= ImGui::Checkbox("Pixel Sampling", (bool*)&(doPixelSampling));
        }
        
        // Pixel sampling
        if (doPixelSampling)
        {
            // Pick pixel sampling method
            ImGui::SeparatorText("Pixel Sampling Method");
            updated |= ImGui::RadioButton("Random point", &samplingMethod, 0); ImGui::SameLine();
            updated |= ImGui::RadioButton("Jittered Grid", &samplingMethod, 1);

            if (!doTAA)
            {
                // Don't give grid sampling as an option when temporal anti aliasing is on since it isn't not random
                ImGui::SameLine();
                updated |= ImGui::RadioButton("Grid", &samplingMethod, 2);
            }
            else if (samplingMethod == 2)
            {
                // If grid sampling, set to default random sampling method
                samplingMethod = 0;
            }

            // Set number of pixel samples
            updated |= ImGui::SliderInt("Samples per pixel", &(samplesPerPixel), 1, 20, samplingMethod == 1 ? "%d^2" : "%d");
        }

        if (updated) onUpdate();
    }

    void fractalMenu(FrameInterpolator *frameInterpolator)
    {
        #define DRAG_FLOAT(label, floatp, speed, min, max, format) ()
        
        bool update = false;
        float coordSpeed = 0.001f;
        const char *coordFormat = "%.6f";

        update |= ImGui::DragInt("Max Iterations", &maxIterations, 1, 0, 100);
        DragFloatKeyframe(frameInterpolator, &update, "Bounding Radius", &boundingRadius, 0.01f, 0.1f, 4.0f, "%.3f");
        DragFloatKeyframe(frameInterpolator, &update, "Escape Threshold", &escapeThreshold, 0.1f, 1.0f, 1000.0f, "%.3f");
        DragFloatKeyframe(frameInterpolator, &update, "z0.w ", &w, coordSpeed, -boundingRadius, boundingRadius, coordFormat);
        
        ImGui::Text("Quaternion Julia Constant c");
        DragFloatKeyframe(frameInterpolator, &update, "##c.x", &c.x, coordSpeed, -4.0f, 4.0f, coordFormat);
        DragFloatKeyframe(frameInterpolator, &update, "##c.y", &c.y, coordSpeed, -4.0f, 4.0f, coordFormat);
        DragFloatKeyframe(frameInterpolator, &update, "##c.z", &c.z, coordSpeed, -4.0f, 4.0f, coordFormat);
        DragFloatKeyframe(frameInterpolator, &update, "##c.w", &c.w, coordSpeed, -4.0f, 4.0f, coordFormat);

        if (update) onUpdate();
    }

    void worldMenu(FrameInterpolator *frameInterpolator)
    {
        bool updated = false;
        
        if (ImGui::Button("Reset time"))
        {
            updated = true;
            u_time = 0.0f;
        }

        updated |= ImGui::ColorEdit3("Background Colour", &backgroundColour.x);

        if (updated) onUpdate();
        
    }

    void materialMenu(FrameInterpolator *frameInterpolator)
    {
        bool update = false;
        bool updateF0 = false;

        DragFloatKeyframe(frameInterpolator, &update, "Roughness", &mat.roughness, 0.01, 0.0, 1.0);
        DragFloatKeyframe(frameInterpolator, &updateF0, "Metallic", &mat.metallic, 0.01, 0.0, 1.0);
        updateF0 |= ImGui::ColorEdit3("Albedo", &(mat.albedo.x));
        
        update |= updateF0;
        if (updateF0) mat.setF0();
        if (update) onUpdate();
    }

    void lightMenu(FrameInterpolator *frameInterpolator)
    {
        bool update = false;

        DragFloatKeyframe(frameInterpolator, &update, "intensity", &light.intensity, 0.1, 0.0, 1000.0);
        update |= ImGui::DragFloat3("Position", &light.position.x, 0.01, -boundingRadius, boundingRadius);
        update |= ImGui::ColorEdit3("Colour", &(light.colour.x));

        if (update) onUpdate();
    }

    void cameraMenu(FrameInterpolator *frameInterpolator)
    {
        bool updateCamera = false;

        DragFloatKeyframe(frameInterpolator, &updateCamera, "Theta", &camera.theta, 0.01, 0.0, 2*PI);
        DragFloatKeyframe(frameInterpolator, &updateCamera, "Phi", &camera.phi, 0.01, 0.1, PI);
        
        DragFloatKeyframe(frameInterpolator, &updateCamera, "Distance", &camera.distance, 0.005, 0.1, 0.0);
        DragFloatKeyframe(frameInterpolator, &updateCamera, "FocalLength", &camera.focalLength, 0.01, 0.1, 100.0);

        if (updateCamera)
        {
            camera.onUpdate();
            onUpdate();
        }
    }

    // * Event callback functions
    void mouseLeftClickCallback(ImVec2 mousePos)
    {
        // onUpdate();
    }
    
    void mouseDragCallback(ImVec2 dpos)
    {
        camera.mouseDragCallback(glm::vec2(dpos.x, dpos.y));
        onUpdate();
    }

    void mouseScrollCallback(float yOffset)
    {
        camera.mouseScrollCallback(yOffset);
        onUpdate();
    }

private:

    Camera camera;
    Shader shader;
    Material mat;
    Light light;

    // States
    int skipAA = 0;
    bool doTemporalAntiAliasing = true;
    bool doTAA = true;
    
    // Renderer settings
    int renderedFrameCount = 0;
    int samplingMethod = 0;
    int samplesPerPixel = 1;
    bool test = false;
    bool doGammaCorrection = true;
    bool doPixelSampling = true;
    glm::ivec2 resolution;

    // Fractal settings
    int maxIterations = 10;
    glm::vec4 c = glm::vec4(-0.2f, 0.6f, 0.2f, 0.2f);
    float w = 0.0f;
    float boundingRadius = 3.0f;
    float escapeThreshold = 100.0f;
    float epsilon = 0.001f;

    // World settings
    glm::vec3 backgroundColour = glm::vec3(0.05);
    float u_time = 0.0;

};

#endif