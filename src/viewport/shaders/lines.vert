#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec4 inColor;

layout(std140, binding = 0) uniform LineUbo {
    mat4 u_mvp;
};

layout(location = 0) out vec4 vColor;

void main() {
    vColor = inColor;
    gl_Position = u_mvp * vec4(inPosition, 1.0);
}
