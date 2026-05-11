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

#define MAX_TEV_STAGES 16

struct TevStage
{
    int TexCoord;
	int TexMap;
	int ColorChannel;

	int ColorA;
	int ColorB;
	int ColorC;
	int ColorD;
	int ColorOp;
	int ColorBias;
	int ColorScale;
	int ColorClamp;
	int ColorOutput;

	int AlphaA;
	int AlphaB;
	int AlphaC;
	int AlphaD;
	int AlphaOp;
	int AlphaBias;
	int AlphaScale;
	int AlphaClamp;
	int AlphaOutput;

	int KonstColorSelector;
	int KonstAlphaSelector;
};

uniform int u_TevStageCount;
uniform TevStage u_TevStages[MAX_TEV_STAGES];

uniform vec4 u_TevColor0;
uniform vec4 u_TevColor1;
uniform vec4 u_TevColor2;

uniform vec4 u_KonstColor0;
uniform vec4 u_KonstColor1;
uniform vec4 u_KonstColor2;
uniform vec4 u_KonstColor3;

const int TEV_ZERO = 0;
const int TEV_ONE = 1;

const int TEV_TEX_COLOR = 2;
const int TEV_TEX_ALPHA = 3;

const int TEV_RAS_COLOR = 4;
const int TEV_RAS_ALPHA = 5;

const int TEV_KONST_COLOR = 6;
const int TEV_KONST_ALPHA = 7;

const int TEV_PREV_COLOR = 8;
const int TEV_PREV_ALPHA = 9;

const int TEV_REG0_COLOR = 10;
const int TEV_REG0_ALPHA = 11;

const int TEV_REG1_COLOR = 12;
const int TEV_REG1_ALPHA = 13;

const int TEV_REG2_COLOR = 14;
const int TEV_REG2_ALPHA = 15;

const int TEV_OP_ADD = 0;
const int TEV_OP_SUB = 1;

const int TEV_OUT_PREV = 0;
const int TEV_OUT_REG0 = 1;
const int TEV_OUT_REG1 = 2;
const int TEV_OUT_REG2 = 3;

uniform vec3 u_LightDirection;
uniform vec3 u_LightColor;
uniform float u_LightIntensity;
uniform vec3 u_AmbiantColor;

out vec4 FragColor;

vec4 SampleTevTexture(int texMap)
{
	if (texMap == 0)
		return texture(u_Texture0, v_TexCoord);

	if (texMap == 1)
		return texture(u_Texture1, v_TexCoord);

	if (texMap == 2)
		return texture(u_Texture2, v_TexCoord);

	return texture(u_Texture, v_TexCoord);
}

vec4 GetKonstColor(int selector)
{
	if (selector == 12) return u_KonstColor0;
	if (selector == 13) return u_KonstColor1;
	if (selector == 14) return u_KonstColor2;
	if (selector == 15) return u_KonstColor3;

	if (selector >= 16 && selector <= 19)
	{
		vec4 k = u_KonstColor0;
		int c = selector - 16;
		float v = c == 0 ? k.r : c == 1 ? k.g : c == 2 ? k.b : k.a;
		return vec4(v);
	}

	if (selector >= 20 && selector <= 23)
	{
		vec4 k = u_KonstColor1;
		int c = selector - 20;
		float v = c == 0 ? k.r : c == 1 ? k.g : c == 2 ? k.b : k.a;
		return vec4(v);
	}

	if (selector >= 24 && selector <= 27)
	{
		vec4 k = u_KonstColor2;
		int c = selector - 24;
		float v = c == 0 ? k.r : c == 1 ? k.g : c == 2 ? k.b : k.a;
		return vec4(v);
	}

	if (selector >= 28 && selector <= 31)
	{
		vec4 k = u_KonstColor3;
		int c = selector - 28;
		float v = c == 0 ? k.r : c == 1 ? k.g : c == 2 ? k.b : k.a;
		return vec4(v);
	}

	return vec4(1.0);
}

vec4 ReadTevArg(
	int arg,
	vec4 texColor,
	vec4 rasColor,
	vec4 konstColor,
	vec4 prev,
	vec4 reg0,
	vec4 reg1,
	vec4 reg2
)
{
	if (arg == TEV_ONE)
		return vec4(1.0);

	if (arg == TEV_TEX_COLOR)
		return vec4(texColor.rgb, 1.0);

	if (arg == TEV_TEX_ALPHA)
		return vec4(texColor.a);

	if (arg == TEV_RAS_COLOR)
		return vec4(rasColor.rgb, 1.0);

	if (arg == TEV_RAS_ALPHA)
		return vec4(rasColor.a);

	if (arg == TEV_KONST_COLOR)
		return vec4(konstColor.rgb, 1.0);

	if (arg == TEV_KONST_ALPHA)
		return vec4(konstColor.a);

	if (arg == TEV_PREV_COLOR)
		return vec4(prev.rgb, 1.0);

	if (arg == TEV_PREV_ALPHA)
		return vec4(prev.a);

	if (arg == TEV_REG0_COLOR)
		return vec4(reg0.rgb, 1.0);

	if (arg == TEV_REG0_ALPHA)
		return vec4(reg0.a);

	if (arg == TEV_REG1_COLOR)
		return vec4(reg1.rgb, 1.0);

	if (arg == TEV_REG1_ALPHA)
		return vec4(reg1.a);

	if (arg == TEV_REG2_COLOR)
		return vec4(reg2.rgb, 1.0);

	if (arg == TEV_REG2_ALPHA)
		return vec4(reg2.a);

	return vec4(0.0);
}

// GX TEV combine formula: result = (d +/- (a*(1-c) + b*c) + bias) * scale
vec4 ApplyTevOp(vec4 a, vec4 b, vec4 c, vec4 d, int op, int bias, int scale, int clampEnabled)
{
	// GX lerp: a*(1-c) + b*c = a + (b-a)*c
	vec4 lerped = a + (b - a) * c;

	// Bias: 0 = +0.0, 1 = +0.5, 2 = -0.5
	vec4 biasVal = vec4(0.0);
	if (bias == 1) biasVal = vec4( 0.5);
	if (bias == 2) biasVal = vec4(-0.5);

	vec4 result;
	if (op == TEV_OP_SUB)
		result = d - lerped + biasVal;
	else
		result = d + lerped + biasVal;

	if (scale == 1)
		result *= 2.0;
	else if (scale == 2)
		result *= 4.0;
	else if (scale == 3)
		result *= 0.5;

	if (clampEnabled == 1)
		return clamp(result, 0.0, 1.0);

	return result;
}

void WriteTevColorOutput(int outputRegister, vec3 rgb, inout vec4 prev, inout vec4 reg0, inout vec4 reg1, inout vec4 reg2)
{
	if (outputRegister == TEV_OUT_REG0)
		reg0.rgb = rgb;
	else if (outputRegister == TEV_OUT_REG1)
		reg1.rgb = rgb;
	else if (outputRegister == TEV_OUT_REG2)
		reg2.rgb = rgb;
	else
		prev.rgb = rgb;
}

void WriteTevAlphaOutput(int outputRegister, float alpha, inout vec4 prev, inout vec4 reg0, inout vec4 reg1, inout vec4 reg2)
{
	if (outputRegister == TEV_OUT_REG0)
		reg0.a = alpha;
	else if (outputRegister == TEV_OUT_REG1)
		reg1.a = alpha;
	else if (outputRegister == TEV_OUT_REG2)
		reg2.a = alpha;
	else
		prev.a = alpha;
}

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

	if (u_TevDebugMode == 11 && u_TevStageCount > 0)
	{
		int checksum = 0;

		int stageCount = min(u_TevStageCount, MAX_TEV_STAGES);

		for (int i = 0; i < stageCount; i++)
		{
			checksum += u_TevStages[i].TexCoord;
			checksum += u_TevStages[i].TexMap;
			checksum += u_TevStages[i].ColorChannel;

			checksum += u_TevStages[i].ColorA;
			checksum += u_TevStages[i].ColorB;
			checksum += u_TevStages[i].ColorC;
			checksum += u_TevStages[i].ColorD;
			checksum += u_TevStages[i].ColorOp;
			checksum += u_TevStages[i].ColorOutput;

			checksum += u_TevStages[i].AlphaA;
			checksum += u_TevStages[i].AlphaB;
			checksum += u_TevStages[i].AlphaC;
			checksum += u_TevStages[i].AlphaD;
			checksum += u_TevStages[i].AlphaOp;
			checksum += u_TevStages[i].AlphaOutput;

			checksum += u_TevStages[i].KonstColorSelector;
			checksum += u_TevStages[i].KonstAlphaSelector;
		}

		float debugValue = fract(float(abs(checksum)) * 0.037);

		baseColor = vec4(debugValue, 1.0 - debugValue, debugValue * 0.5, 1.0);
	}

	if ((u_TevDebugMode == 0 || u_TevDebugMode == 12 || (u_TevDebugMode >= 20 && u_TevDebugMode <= 23)) && u_TevStageCount > 0)
	{
		vec4 prev = vec4(0.0, 0.0, 0.0, 0.0);
		vec4 reg0 = u_TevColor0;
		vec4 reg1 = u_TevColor1;
		vec4 reg2 = u_TevColor2;

		// Compute Lambert lighting for use as rasterized color (rasc).
		// In GX, rasc is the per-vertex rasterized color/lighting result.
		// color_channel == 0xFF (255) means GX_COLOR_NULL -> rasc = (0,0,0,0).
		// color_channel 0/1 = vertex color channels (unused here -> use Lambert).
		// We bake the Lambert result into rasColor so TEV math is self-contained.
		vec3 normal    = normalize(v_Normal);
		vec3 lightDir  = normalize(u_LightDirection);
		float ndotl    = max(dot(normal, lightDir), 0.0);
		vec3 lambertRGB = u_AmbiantColor + u_LightColor * ndotl * u_LightIntensity;
		
		// Alpha channel of rasColor: GX uses alpha from rasterization (typically 1.0)
		vec4 lambertColor = vec4(lambertRGB, 1.0);

		int stageCount = min(u_TevStageCount, MAX_TEV_STAGES);

		bool debugStageWritten = false;

		for (int i = 0; i < stageCount; i++)
		{
			// Determine rasColor for this stage based on color_channel.
			// GX_COLOR_NULL (stored as -1 from OKMAT loader, or 255 raw) -> rasc = (0,0,0,0)
			// GX_COLOR_ZERO (6) -> rasc = (0,0,0,0)
			// Any active channel (GX_COLOR0A0=4, GX_COLOR0=0, etc.) -> use Lambert result
			bool rasActive = (u_TevStages[i].ColorChannel >= 0 &&
			                  u_TevStages[i].ColorChannel != 255 &&
			                  u_TevStages[i].ColorChannel != 6);
			vec4 rasColor = rasActive ? lambertColor : vec4(0.0);

			vec4 texColor  = SampleTevTexture(u_TevStages[i].TexMap);
			vec4 konstColor = GetKonstColor(u_TevStages[i].KonstColorSelector);
			vec4 konstAlpha = GetKonstColor(u_TevStages[i].KonstAlphaSelector);

			vec4 ca = ReadTevArg(u_TevStages[i].ColorA, texColor, rasColor, konstColor, prev, reg0, reg1, reg2);
			vec4 cb = ReadTevArg(u_TevStages[i].ColorB, texColor, rasColor, konstColor, prev, reg0, reg1, reg2);
			vec4 cc = ReadTevArg(u_TevStages[i].ColorC, texColor, rasColor, konstColor, prev, reg0, reg1, reg2);
			vec4 cd = ReadTevArg(u_TevStages[i].ColorD, texColor, rasColor, konstColor, prev, reg0, reg1, reg2);

			vec4 colorResult = ApplyTevOp(ca, cb, cc, cd,
				u_TevStages[i].ColorOp,
				u_TevStages[i].ColorBias,
				u_TevStages[i].ColorScale,
				u_TevStages[i].ColorClamp);

			vec4 aa = ReadTevArg(u_TevStages[i].AlphaA, texColor, rasColor, konstAlpha, prev, reg0, reg1, reg2);
			vec4 ab = ReadTevArg(u_TevStages[i].AlphaB, texColor, rasColor, konstAlpha, prev, reg0, reg1, reg2);
			vec4 ac = ReadTevArg(u_TevStages[i].AlphaC, texColor, rasColor, konstAlpha, prev, reg0, reg1, reg2);
			vec4 ad = ReadTevArg(u_TevStages[i].AlphaD, texColor, rasColor, konstAlpha, prev, reg0, reg1, reg2);

			vec4 alphaResult = ApplyTevOp(aa, ab, ac, ad,
				u_TevStages[i].AlphaOp,
				u_TevStages[i].AlphaBias,
				u_TevStages[i].AlphaScale,
				u_TevStages[i].AlphaClamp);

			// In GX, color and alpha outputs are written to separate registers independently.
			WriteTevColorOutput(u_TevStages[i].ColorOutput, colorResult.rgb, prev, reg0, reg1, reg2);

			bool alphaStageIsDisabled =
				u_TevStages[i].AlphaA == TEV_ZERO &&
				u_TevStages[i].AlphaB == TEV_ZERO &&
				u_TevStages[i].AlphaC == TEV_ZERO &&
				u_TevStages[i].AlphaD == TEV_ZERO;

			if (!alphaStageIsDisabled)
				WriteTevAlphaOutput(u_TevStages[i].AlphaOutput, alphaResult.r, prev, reg0, reg1, reg2);

			if (u_TevDebugMode >= 20 && u_TevDebugMode <= 23)
			{
				int debugStage = u_TevDebugMode - 20;

				if (i == debugStage)
				{
					baseColor = prev;
					debugStageWritten = true;
					break;
				}
			}
		}

		if (!debugStageWritten)
			baseColor = prev;
	}

    if (u_UseAlphaCutout == 1 && baseColor.a < u_AlphaCutoff)
        discard;

    // When using the TEV pipeline (TevStageCount > 0), lighting is already folded into
    // rasColor inside the TEV loop. Applying Lambert again here would double the lighting.
    // For non-TEV materials (no stages), we apply Lambert as a simple diffuse pass.
    if (u_TevStageCount > 0)
    {
        FragColor = baseColor;
    }
    else
    {
        vec3 normal   = normalize(v_Normal);
        vec3 lightDir = normalize(u_LightDirection);
        float ndotl   = max(dot(normal, lightDir), 0.0);
        vec3 lighting = u_AmbiantColor + (u_LightColor * ndotl * u_LightIntensity);
        FragColor = vec4(baseColor.rgb * lighting, baseColor.a);
    }
}