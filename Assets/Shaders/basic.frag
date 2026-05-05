#version 330 core

in vec2 v_TexCoord;
in vec3 v_Normal;

uniform sampler2D u_Texture;
uniform int u_UseAlphaCutout;

uniform vec3 u_LightDirection;
uniform vec3 u_LightColor;
uniform float u_LightIntensity;
uniform vec3 u_AmbiantColor;

out vec4 FragColor;

void main()
{
    vec4 texColor = texture(u_Texture, v_TexCoord);

    if (u_UseAlphaCutout == 1 && texColor.a < 0.1)
        discard;

    vec3 normal = normalize(v_Normal);
    vec3 lightDir = normalize(u_LightDirection);

    float diffuse = max(dot(normal, lightDir), 0.0);

    vec3 lighting = u_AmbiantColor + (u_LightColor * u_LightIntensity * diffuse);
    vec3 finalColor = texColor.rgb * lighting;

    FragColor = vec4(finalColor, 1.0);
}