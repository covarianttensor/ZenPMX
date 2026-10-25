#version 450

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

layout(location = 0) out vec4 fragColor;

void main() {
    fragColor = u_edgeColor;
}
