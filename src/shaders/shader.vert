#version 450 core

layout (binding = 1) uniform uniformBufferObject {
	mat4 mvp;
} ubo;

layout (location = 0) in vec2 inPosition;
layout (location = 1) in vec2 inTexCoord;

layout (location = 0) out vec2 texCoord;

void main() {
	gl_Position = ubo.mvp * vec4(inPosition, 0.0, 1.0);
	// gl_Position = vec4(inPosition, 0.0, 1.0);
	texCoord = inTexCoord;
}
