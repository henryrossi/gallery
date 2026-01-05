#version 450 core

layout (push_constant) uniform Globals {
    vec2 resolution;
} globals;

layout(location = 0) in vec2 pos0; // top left corner
layout(location = 1) in vec2 pos1; // bottom right corner
layout(location = 2) in vec2 src0;
layout(location = 3) in vec2 src1;
layout(location = 4) in uint srcID;
layout(location = 5) in vec4 inColor0;
layout(location = 6) in vec4 inColor1;
layout(location = 7) in vec4 inColor2;
layout(location = 8) in vec4 inColor3;

layout(location = 0) out vec4 outColor;
layout(location = 1) out uint texID;
layout(location = 2) out vec2 uv;

vec2 vertices[4] = vec2[](
    vec2(-1.0, -1.0),
    vec2(-1.0, 1.0),
    vec2(1.0, -1.0),
    vec2(1.0, 1.0)
);

void main() {
	vec4 colors[4] = vec4[4](
	        inColor0,
		inColor1,
		inColor2,
		inColor3
	);

	vec2 dstHalfSize = (pos1 - pos0) / 2;
        vec2 dstCenter = (pos1 + pos0) / 2;
	vec2 dstPos = (vertices[gl_VertexIndex] * dstHalfSize) + dstCenter;

	gl_Position = vec4(2 * dstPos.x / globals.resolution.x - 1,
			   2 * dstPos.y / globals.resolution.y - 1, 
		           0.0, 1.0);
	
	vec2 srcHalfSize = (src1 - src0) / 2;
	vec2 srcCenter = (src1 + src0) / 2;
	uv = (vertices[gl_VertexIndex] * srcHalfSize) + srcCenter;

        outColor = colors[gl_VertexIndex];
	texID = srcID;
}
