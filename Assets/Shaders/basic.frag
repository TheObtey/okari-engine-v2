#version 330 core

in vec2 v_TexCoord;

uniform sampler2D u_Texture;
uniform int u_UseAlphaCutout;

out vec4 FragColor;

void main()
{
    vec4 texColor = texture(u_Texture, v_TexCoord);

    if (u_UseAlphaCutout == 1 && texColor.a < 0.1)
        discard;

    FragColor = vec4(texColor.rgb, 1.0);
}