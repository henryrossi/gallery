#version 450 core
#extension GL_EXT_nonuniform_qualifier : require

layout(binding = 0) uniform texture2D textures[];
layout(binding = 1) uniform sampler samp;

layout(location = 0) in vec4 inColor;
layout(location = 1) in flat uint texID;
layout(location = 2) in vec2 uv;
layout(location = 3) in vec2 dstPos;
layout(location = 4) in vec2 dstCenter;
layout(location = 5) in vec2 dstHalfSize;
layout(location = 6) in float cornerRadius;
layout(location = 7) in float edgeSoftness;


layout(location = 0) out vec4 outColor;

float rounded_rect_SDF(vec2 samplePos, vec2 rectCenter, vec2 rectHalfSize, 
		       float r) {
	vec2 d2 = (abs(rectCenter - samplePos) - rectHalfSize + vec2(r, r));
	return min(max(d2.x, d2.y), 0.0) + length(max(d2, 0.0)) - r;
}

void main() {
	vec2 softnessPadding = vec2(max(0, edgeSoftness * 2 - 1),
				    max(0, edgeSoftness * 2 - 1));
	float dist = rounded_rect_SDF(dstPos, dstCenter, dstHalfSize - 
				     softnessPadding, cornerRadius);
	float sdfFactor = 1.0 - smoothstep(0, 2 * edgeSoftness, dist);

	vec4 t = texture(nonuniformEXT(sampler2D(textures[texID], samp)), uv);

	outColor = t * inColor * sdfFactor;
}
