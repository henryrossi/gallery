#version 450

layout (binding = 1) uniform uniformBufferObject {
	float xAdjustment;
	float yAdjustment;
} ubo;

layout (location = 0) in vec2 inPosition;
layout (location = 1) in vec2 inTexCoord;

layout (location = 0) out vec2 texCoord;

void main() {
	vec2 adj = vec2(ubo.xAdjustment, ubo.yAdjustment);
	gl_Position = vec4(adj * inPosition, 0.0, 1.0);
	texCoord = inTexCoord;
}
