#version 450 core

layout(binding = 0) uniform QuadUniform {
	mat4 scale;
	mat4 proj;
	vec3 color;
	vec2 trans;
} ubo;

layout (location = 0) in vec2 inPosition;

layout (location = 0) out vec3 color;

void main() {
	vec4 worldPos =  ubo.scale * vec4(inPosition, 0.0, 1.0);
	vec4 pos = ubo.proj * worldPos;
	color = ubo.color;
	gl_Position = pos;
}
