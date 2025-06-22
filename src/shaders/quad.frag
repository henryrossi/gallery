#version 450 core

layout(location = 0) in vec4 inPosition;

layout(location = 0) out vec4 outColor;

void main() {
	outColor = vec4(inPosition.r, inPosition.g, inPosition.b, 0.2f);
}
