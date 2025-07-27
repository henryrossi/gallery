#version 450 core

layout(binding = 0) uniform QuadUniform {
	mat4 mvp;
	vec3 color;
} ubo;

layout (location = 0) in vec2 inPosition;

layout (location = 0) out vec3 color;

void main() {
	gl_Position = ubo.mvp * vec4(inPosition, 0.0, 1.0);
	color = ubo.color;
}
