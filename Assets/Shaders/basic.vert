#version 330 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec2 a_TexCoord;
layout(location = 2) in vec3 a_Normal;

uniform mat4 u_Model;
uniform mat4 u_MVP;

out vec2 v_TexCoord;
out vec3 v_Normal;

void main()
{
	v_TexCoord = a_TexCoord;

	mat3 normalMatrix = transpose(inverse(mat3(u_Model)));
	v_Normal = normalize(normalMatrix * a_Normal);

	gl_Position = u_MVP * vec4(a_Position, 1.0);
}
