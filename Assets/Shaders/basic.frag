#version 330 core

in vec2 v_TexCoord;
in vec3 v_Normal;

uniform sampler2D u_Texture;
uniform sampler2D u_Texture0;
uniform sampler2D u_Texture1;
uniform sampler2D u_Texture2;

uniform int u_UseAlphaCutout;
uniform float u_AlphaCutoff;

uniform int u_TevDebugMode;
uniform int u_TextureSlotCount;

uniform vec4 u_TevColor0;
uniform vec4 u_TevColor1;
uniform vec4 u_TevColor2;

uniform vec4 u_KonstColor0;
uniform vec4 u_KonstColor1;
uniform vec4 u_KonstColor2;
uniform vec4 u_KonstColor3;

uniform vec3 u_LightDirection;
uniform vec3 u_LightColor;
uniform float u_LightIntensity;
uniform vec3 u_AmbiantColor;

out vec4 FragColor;

void main()
{
    vec4 baseColor = texture(u_Texture, v_TexCoord);
    
    if (u_TevDebugMode == 1 && u_TextureSlotCount > 0)
        baseColor = texture(u_Texture0, v_TexCoord);
    
    if (u_TevDebugMode == 2 && u_TextureSlotCount > 1)
        baseColor = texture(u_Texture1, v_TexCoord);
    
    if (u_TevDebugMode == 3 && u_TextureSlotCount > 2)
        baseColor = texture(u_Texture2, v_TexCoord);
    
    if (u_TevDebugMode == 4)
        baseColor = u_TevColor0;

    if (u_TevDebugMode == 5)
        baseColor = u_TevColor1;

    if (u_TevDebugMode == 6)
        baseColor = u_TevColor2;

    if (u_TevDebugMode == 7)
        baseColor = u_KonstColor0;

    if (u_TevDebugMode == 8)
        baseColor = u_KonstColor1;

    if (u_TevDebugMode == 9)
        baseColor = u_KonstColor2;

    if (u_TevDebugMode == 10)
        baseColor = u_KonstColor3;

    if (u_UseAlphaCutout == 1 && baseColor.a < u_AlphaCutoff)
        discard;

    vec3 normal = normalize(v_Normal);
    vec3 lightDir = normalize(u_LightDirection);

    float ndot1 = max(dot(normal, lightDir), 0.0);

    vec3 lighting = u_AmbiantColor + (u_LightColor * ndot1 * u_LightIntensity);

    FragColor = vec4(baseColor.rgb * lighting, baseColor.a);
}