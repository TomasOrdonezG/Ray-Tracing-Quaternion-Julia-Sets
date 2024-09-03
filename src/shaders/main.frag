#version 330 core

// * Inputs / Outputs
in vec2 TexCoords;
out vec4 FragColour;

// * Macrodefinitions
#define FLOAT_MAX 3.402823466e+38
#define FLOAT_MIN 1.175494351e-38
#define PI 3.14159265358979323846

// * Structs
struct Ray { vec3 pos, dir; };

// * Rendering Uniforms
uniform bool test;
uniform bool doPixelSampling;
uniform int samplingMethod;
uniform bool doGammaCorrection;
uniform bool doTemporalAntiAliasing;
uniform int renderedFrameCount;
uniform int samplesPerPixel;
uniform ivec2 resolution;
uniform sampler2D prevFrameTexture;

// * Fractal Uniforms
uniform int maxIterations;
uniform vec4 c;
uniform float w;
uniform float boundingRadius2;
uniform float escapeThreshold;
uniform float epsilon;

// * World Uniforms
uniform float u_time;
uniform vec3 backgroundColour;

// * Camera Uniforms
uniform vec3 lookfrom;
uniform vec3 pixelDW;
uniform vec3 pixelDH;
uniform vec3 viewportOrigin;
uniform float cameraDistance;

// * Material Uniforms
uniform float roughness;
uniform float metallic;
uniform vec3 albedo;
uniform vec3 F0;

// * Light Uniforms
uniform vec3 lightPos;
uniform vec3 lightColour;
uniform float lightIntensity;

// * Utility functions
vec3 rayAt(Ray ray, float t)
{
    return ray.pos + t*ray.dir;
}

float length2(vec3 v)
{
    return dot(v, v);
}

float rand()
{
    return fract(sin(dot(gl_FragCoord.xy, vec2(12.9898, 78.233)) * u_time) * 43758.5453);
}

vec3 gammaUncorrect(vec3 rgb)
{
    return pow(rgb, vec3(2.2));
}

vec3 gammaCorrect(vec3 linear)
{
    return pow(linear, vec3(1.0/2.2));
}

vec3 postProcess(vec3 colour)
{

    if (doGammaCorrection)
    {
        colour = gammaCorrect(colour);
    }
    
    if (doTemporalAntiAliasing)
    {
        // Average colour with previous frame
        vec3 prevColour = texture(prevFrameTexture, TexCoords).xyz;
        colour = mix(prevColour, colour, 1.0 / (renderedFrameCount + 1));
    }

    return colour;

}

// * Physically Based Renderer
vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return num / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float num = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return num / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

vec3 PBR(vec3 N, vec3 P, vec3 V)
{
    // Convert colours to linear space
    vec3 albedo_linear = albedo;
    vec3 lightColour_linear = lightColour;
    if (doGammaCorrection)
    {
        albedo_linear = gammaUncorrect(albedo);
        lightColour_linear = gammaUncorrect(lightColour);
    }

    vec3 Lo = vec3(0.0);
    // for light in lights
    // {
    vec3 L = normalize(lightPos - P);
    vec3 H = normalize(V + L);
    float NdotL = max(dot(N, L), 0.0);  

    float dist = length(lightPos - P);
    float attenuation = 1.0 / (dist * dist);
    vec3 radiance = lightColour_linear * lightIntensity * attenuation;

    // Cook-Torrance BRDF
    float NDF = DistributionGGX(N, H, roughness);       
    float G = GeometrySmith(N, V, L, roughness);      
    vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);
    
    vec3 num = NDF * G * F;
    float denom = 4.0 * max(dot(N, V), 0.0) * NdotL + 0.0001;
    vec3 specular = num / denom;

    // Calculate reflective and refractive indexes
    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metallic; 

    Lo += (kD * albedo_linear / PI + specular) * radiance * NdotL;
    // }

    return Lo;

    // ? Ambient lighting
    // float ao = 1.0; // Ambient occlusion
    // vec3 ambient = vec3(0.03) * albedo_linear * ao;
    // return ambient + Lo;
}

// * Quaternion operations
vec4 qSquare(vec4 q)
{
	vec4 r;
	r.x = q.x*q.x - dot(q.yzw, q.yzw);
	r.yzw = 2*q.x*q.yzw;
	return r;
}

vec4 qMultiply(vec4 q1, vec4 q2)
{
	vec4 r;
	r.x = q1.x*q2.x - dot(q1.yzw, q2.yzw);
	r.yzw = q1.x*q2.yzw + q2.x*q1.yzw + cross(q1.yzw, q2.yzw);
	return r;
}

// * Colour calculation
float hitSphere(Ray r)
{
    vec3 oc = -r.pos;
    float a = length2(r.dir);
    float h = dot(r.dir, oc);
    float c = length2(oc) - boundingRadius2;
    float discriminant = h*h - a*c;

    if (discriminant < 0)
        return -1.0;
    else
        return (h - sqrt(discriminant)) / a;
}

float juliaDistanceEstimate(vec4 z, vec4 dz)
{
    float lenZ = length(z);
    return 0.5 * log(lenZ) * (lenZ / length(dz));
}

void juliaRecurrence(inout vec4 z, inout vec4 dz)
{
    for (int i = 0; dot(z, z) < escapeThreshold && i < maxIterations; i++)
    {
        // dz = 2z_0*dz_0
        dz = 2.0*qMultiply(z, dz);

        // z = z_0^2 + c
        z = qSquare(z) + c;
    }
}

vec3 surfaceNormal(vec3 p, float julia_w)
{
    // Assuming w is defined correctly in the larger scope
    // Convert 3D point to a Quaternion
    vec4 qP = vec4(p, julia_w);

    float delta = 0.000001;

    // Perturbed points in the x, y, z direction by delta
    float gradX, gradY, gradZ;

    vec4 gx1 = qP - vec4(delta, 0, 0, 0);
    vec4 gx2 = qP + vec4(delta, 0, 0, 0);
    vec4 gy1 = qP - vec4(0, delta, 0, 0);
    vec4 gy2 = qP + vec4(0, delta, 0, 0);
    vec4 gz1 = qP - vec4(0, 0, delta, 0);
    vec4 gz2 = qP + vec4(0, 0, delta, 0);

    // Calculate Julia set iteration on perturbed points
    for (int i = 0; i < maxIterations; i++)
    {
        if (dot(gx1, gx1) < escapeThreshold) gx1 = qSquare(gx1) + c;
        if (dot(gx2, gx2) < escapeThreshold) gx2 = qSquare(gx2) + c;
        if (dot(gy1, gy1) < escapeThreshold) gy1 = qSquare(gy1) + c;
        if (dot(gy2, gy2) < escapeThreshold) gy2 = qSquare(gy2) + c;
        if (dot(gz1, gz1) < escapeThreshold) gz1 = qSquare(gz1) + c;
        if (dot(gz2, gz2) < escapeThreshold) gz2 = qSquare(gz2) + c;
    }

    // Gradient approximation
    gradX = length(gx2) - length(gx1);
    gradY = length(gy2) - length(gy1);
    gradZ = length(gz2) - length(gz1);

    // Return normal of the approximated gradient
    vec3 N = vec3(gradX, gradY, gradZ);
    N = normalize(N);

    return N;
}

bool intersectJulia(Ray ray, float julia_w, out vec3 normal, out vec3 intersectionPoint)
{
    // Test ray at different points until an intersection is found
    float distanceEstimate, rayLength = 1.0;
    while (length2(rayAt(ray, rayLength)) < boundingRadius2)
    {
        // Initial z value and its derivative
        vec4 z = vec4(rayAt(ray, rayLength), julia_w);
        vec4 dz = vec4(1.0, 0.0, 0.0, 0.0);

        // Run escape time algorithm for Julia set
        juliaRecurrence(z, dz);
        
        // Check for intersection
        distanceEstimate = juliaDistanceEstimate(z, dz);
        if (distanceEstimate < epsilon)
        {
            // Handle intersection
            intersectionPoint = rayAt(ray, rayLength);
            normal = surfaceNormal(intersectionPoint, julia_w);
            return true;
        }
            
        // If there is no intersection, then update ray length and run again
        rayLength += distanceEstimate;
    }


    return false;
}

vec3 calculateColour(vec2 coord)
{
    // Convert colour to linear space
    vec3 backgroundColour_linear = backgroundColour;
    if (doGammaCorrection) backgroundColour_linear = gammaUncorrect(backgroundColour);
    
    // Calculate w
    float julia_w = w;
    // if (test) julia_w = -1.0 + u_time/10.0;
    
    // Create ray from camera
    vec3 pixelSample = viewportOrigin + (coord.x*pixelDW) + (coord.y*pixelDH);
    Ray ray = Ray(lookfrom, normalize(pixelSample - lookfrom));

    // Begin ray from bounding sphere's surface
    ray.pos = rayAt(ray, hitSphere(ray));

    // Check for julia intersection
    vec3 N, P;
    if (!intersectJulia(ray, julia_w, N, P)) return backgroundColour_linear;

    // Calculate colour using preferred rendering method
    vec3 finalColour = PBR(N, P, -ray.dir);
    return finalColour;
}

// * Pixel sampling methods
vec3 randomPointSample()
{
    vec3 colour = vec3(0.0);
    vec2 pixelCenter = gl_FragCoord.xy + 0.5;

    for (int i = 0; i < samplesPerPixel; i++)
    {
        // Sample random window coords
        vec2 offset = vec2(rand() - 0.5, rand() - 0.5);
        vec2 sampledCoord = pixelCenter + offset;

        // Calculate colour
        colour += calculateColour(sampledCoord);
    }

    return colour / float(samplesPerPixel);
}

vec3 gridSample()
{
    vec3 colour = vec3(0.0);

    for (int i = 0; i < samplesPerPixel; i++)
    {
        for (int j = 0; j < samplesPerPixel; j++)
        {
            // Sample random window coords
            vec2 offset = (vec2(i, j) + 0.5) / float(samplesPerPixel);
            vec2 sampledCoord = gl_FragCoord.xy + offset;

            // Calculate colour
            colour += calculateColour(sampledCoord);
        }
    }

    return colour / float(samplesPerPixel*samplesPerPixel);
}

vec3 jitteredGridSample()
{
    vec3 colour = vec3(0.0);

    for (int i = 0; i < samplesPerPixel; i++)
    {
        for (int j = 0; j < samplesPerPixel; j++)
        {
            // Sample random window coords with jitter
            vec2 offset = (vec2(i, j) + vec2(rand(), rand())) / float(samplesPerPixel);
            vec2 sampledCoord = gl_FragCoord.xy + offset;

            // Calculate colour
            colour += calculateColour(sampledCoord);
        }
    }

    return colour / float(samplesPerPixel*samplesPerPixel);
}

void main()
{

    vec3 currentColour;

    if (doTemporalAntiAliasing || doPixelSampling)
    {
        // Sample pixel based on some sampling method
        if (samplingMethod == 0)
            currentColour = randomPointSample();
        else if (samplingMethod == 1)
            currentColour = jitteredGridSample();
        else if (samplingMethod == 2)
            currentColour = gridSample();
    }
    else
    {
        // No sampling, calculate colour at the pixel's center
        currentColour = calculateColour(gl_FragCoord.xy + 0.5);
    }

    currentColour = postProcess(currentColour);
    FragColour = vec4(currentColour, 1.0);
}