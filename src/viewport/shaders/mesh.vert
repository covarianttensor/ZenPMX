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

layout(location = 0) out vec3 vNormal;
layout(location = 1) out vec2 vUV;
layout(location = 2) out vec2 vSphereUV;
layout(location = 3) out vec3 vViewPos;

void main() {
    vec3 inPosition = inPosScale.xyz;
    vec3 inNormal = inNormalPad.xyz;
    vec2 inUV = inUVPad.xy;

    vec3 nView = mat3(u_normalMatrix) * inNormal;
    float nLen = length(nView);
    vec3 normView = (nLen > 1e-4) ? (nView / nLen) : vec3(0.0, 1.0, 0.0);

    vSphereUV = vec2(0.5 + 0.5 * normView.x, 0.5 - 0.5 * normView.y);
    vNormal = normView;
    vUV = inUV;
    vec4 viewPos = u_modelView * vec4(inPosition, 1.0);
    vViewPos = viewPos.xyz;
    gl_Position = u_mvp * vec4(inPosition, 1.0);
}
