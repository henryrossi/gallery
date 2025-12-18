#version 450 core

layout (location = 0) out vec3 color;

vec2 vertices[4] = vec2[](
    vec2(-1.0, -1.0),
    vec2(-1.0, 1.0),
    vec2(1.0, -1.0),
    vec2(1.0, 1.0)
);

void main() {
	gl_Position = vec4(vertices[gl_VertexIndex], 0.0, 1.0);
	color = vec3(1.0, 0.5, 0.0);
}
