#version 450 core

layout (location = 0) in vec2 inPosition;

layout (location = 0) out  vec4 outPosition;

void main() {
	gl_Position = vec4(inPosition, 0.0, 1.0);
	outPosition = vec4(inPosition, 0.0, 1.0);
}
