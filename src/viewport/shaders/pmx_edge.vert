#version 450

layout(location = 0) in vec4 inPosScale;
layout(location = 1) in vec4 inNormalPad;
layout(location = 2) in vec4 inUVPad;

layout(std140, binding = 0) uniform FrameAndMaterialUbo {
    mat4 u_mvp;
    mat4 u_modelView;
    mat4 u_normalMatrix;
    vec4 u_lightDir;          // Normalized directional light in view space
    vec4 u_lightColor;        // Default (0.6, 0.6, 0.6, 1.0)
    vec4 u_cameraPos;

    // Material base properties
    vec4 u_diffuse;           // RGBA
    vec4 u_specular;          // RGB + A=specularPower
    vec4 u_ambient;           // RGB
    vec4 u_edgeColor;         // RGBA
    vec4 u_edgeParams;        // x=edgeSize, y=screenHeight, z=sphereMode (0..3), w=hasToon (0/1)
    vec4 u_texTintMul;
    vec4 u_texTintAdd;
    vec4 u_sphereTintMul;
    vec4 u_sphereTintAdd;
    vec4 u_toonTintMul;
    vec4 u_toonTintAdd;
};

void main() {
    vec3 N = inNormalPad.xyz;
    float len = length(N);
    vec3 safeNorm = (len > 1e-4) ? (N / len) : vec3(0.0);

    float edgeScale = inPosScale.w;
    float rawOffset = u_edgeParams.x * edgeScale * 0.01;
    float clampedOffset = clamp(rawOffset, -10.0, 10.0);

    vec3 extrudedPos = inPosScale.xyz + safeNorm * clampedOffset;
    gl_Position = u_mvp * vec4(extrudedPos, 1.0);
}
