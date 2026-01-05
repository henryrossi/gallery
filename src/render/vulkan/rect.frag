#version 450 core
#extension GL_EXT_nonuniform_qualifier : require

layout(binding = 0) uniform texture2D textures[];
layout(binding = 1) uniform sampler samp;

layout(location = 0) in vec4 inColor;
layout(location = 1) in flat uint texID;
layout(location = 2) in vec2 uv;

layout(location = 0) out vec4 outColor;

void main() {
	vec4 t = texture(nonuniformEXT(sampler2D(textures[texID], samp)), uv);
	outColor = t * inColor;
}
