#version 450

layout(location = 0) in vec3 vNormal;
layout(location = 1) in vec2 vUV;
layout(location = 2) in vec2 vSphereUV;
layout(location = 3) in vec3 vViewPos;

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

layout(binding = 1) uniform sampler2D u_diffuseTex;
layout(binding = 2) uniform sampler2D u_toonTex;
layout(binding = 3) uniform sampler2D u_sphereTex;

layout(location = 0) out vec4 outColor;

void main() {
    // 1. Normal and Light vectors in view space
    vec3 N = normalize(vNormal);
    if (!gl_FrontFacing) N = -N;
    vec3 L = normalize(u_lightDir.xyz);
    if (dot(L, L) < 0.0001) {
        L = normalize(vec3(0.5, 1.0, 0.5));
    }

    // 2. Diffuse texture sampling and modulation
    vec4 texCol = texture(u_diffuseTex, vUV) * u_texTintMul + u_texTintAdd;
    vec4 baseColor = vec4(u_diffuse.rgb * texCol.rgb, u_diffuse.a * texCol.a);

    // 3. Toon Ramp evaluation
    float NdotL = dot(N, L);
    float toonV = clamp(0.5 - 0.5 * dot(N, L), 0.0, 1.0);
    vec4 toonCol = texture(u_toonTex, vec2(0.5, toonV)) * u_toonTintMul + u_toonTintAdd;

    vec3 lightFactor;
    if (u_edgeParams.w > 0.5) { // hasToon
        lightFactor = u_ambient.rgb + u_lightColor.rgb * toonCol.rgb;
    } else {
        lightFactor = u_ambient.rgb + u_lightColor.rgb * max(NdotL, 0.0);
    }
    vec3 color = baseColor.rgb * lightFactor;

    // 4. Sphere Map evaluation
    int sphereMode = int(u_edgeParams.z + 0.5);
    if (sphereMode == 1 || sphereMode == 2) {
        vec4 sphereCol = texture(u_sphereTex, vSphereUV) * u_sphereTintMul + u_sphereTintAdd;
        if (sphereMode == 1) { // Multiplication (.sph)
            color.rgb *= sphereCol.rgb;
        } else if (sphereMode == 2) { // Addition (.spa)
            color.rgb += sphereCol.rgb;
        }
    }

    // 5. Blinn-Phong Specular
    vec3 V = (length(vViewPos) > 0.0001) ? normalize(-vViewPos) : vec3(0.0, 0.0, 1.0);
    vec3 H = normalize(L + V);
    float NdotH = max(dot(N, H), 0.0);
    float specPower = max(u_specular.a, 1.0);
    float specFactor = (NdotL > 0.0) ? pow(NdotH, specPower) : 0.0;
    vec3 specular = u_specular.rgb * u_lightColor.rgb * specFactor;
    color.rgb += specular;

    // 6. Final composite
    outColor = vec4(color.rgb, baseColor.a);
}
