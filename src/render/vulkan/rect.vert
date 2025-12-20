#version 450 core

layout (binding = 0) uniform VSInput {
    vec2 resolution; // should be global
    vec2 pos0; // top left corner
    vec2 pos1; // bottom right corner
} vsInput;

layout (location = 0) out vec3 color;

vec2 vertices[6] = vec2[](
    vec2(-1.0, -1.0),
    vec2(-1.0, 1.0),
    vec2(1.0, -1.0),
    vec2(1.0, 1.0),
    vec2(1.0, -1.0),
    vec2(-1.0, 1.0)
);

void main() {
	gl_Position = vec4(vertices[gl_VertexIndex], 0.0, 1.0);
	color = vec3(1.0, 0.5, 0.0);
}
