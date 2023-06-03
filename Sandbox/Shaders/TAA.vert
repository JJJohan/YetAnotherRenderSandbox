#version 460

// Silly hack, use specialisation constants when pipeline manager & render graph are implemented.
layout(push_constant) uniform PushConsts {
	uint enabled;
} pushConsts;

layout(location = 0) out vec2 fragUv;
layout(location = 1) flat out uint fragEnabled;

void main()
{
	vec2 vertices[3] = vec2[3](vec2(-1.0, 3.0), vec2(3.0, -1.0), vec2(-1.0, -1.0));
	gl_Position = vec4(vertices[gl_VertexIndex], 0.0, 1.0);
	fragUv = 0.5 * gl_Position.xy + vec2(0.5);
	fragEnabled = pushConsts.enabled;
}