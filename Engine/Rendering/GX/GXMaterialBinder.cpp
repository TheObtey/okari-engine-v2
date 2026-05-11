#include "Rendering/GX/GXMaterialBinder.h"

#include <algorithm>

namespace Okari
{
	static int ToInt(GXTevColorArg value)
	{
		return static_cast<int>(value);
	}

	static int ToInt(GXTevRegister value)
	{
		return static_cast<int>(value);
	}

	static int ToInt(GXTevOp value)
	{
		return static_cast<int>(value);
	}

	void GXMaterialBinder::BindMaterialUniforms(const Shader& shader, const Material& material)
	{
		shader.SetVec4("u_TevColor0", material.TevColor0);
		shader.SetVec4("u_TevColor1", material.TevColor1);
		shader.SetVec4("u_TevColor2", material.TevColor2);

		shader.SetVec4("u_KonstColor0", material.KonstColor0);
		shader.SetVec4("u_KonstColor1", material.KonstColor1);
		shader.SetVec4("u_KonstColor2", material.KonstColor2);
		shader.SetVec4("u_KonstColor3", material.KonstColor3);

		const int stageCount = static_cast<int>(std::min<size_t>(material.TevStages.size(), 16));
		shader.SetInt("u_TevStageCount", stageCount);

		for (int i = 0; i < stageCount; i++)
		{
			const GXTevStage& stage = material.TevStages[i];
			const std::string prefix = "u_TevStages[" + std::to_string(i) + "]";

			shader.SetInt(prefix + ".TexCoord", stage.Order.TexCoord);
			shader.SetInt(prefix + ".TexMap", stage.Order.TexMap);
			shader.SetInt(prefix + ".ColorChannel", stage.Order.ColorChannel);

			shader.SetInt(prefix + ".ColorA", ToInt(stage.ColorStage.A));
			shader.SetInt(prefix + ".ColorB", ToInt(stage.ColorStage.B));
			shader.SetInt(prefix + ".ColorC", ToInt(stage.ColorStage.C));
			shader.SetInt(prefix + ".ColorD", ToInt(stage.ColorStage.D));
			shader.SetInt(prefix + ".ColorOp", ToInt(stage.ColorStage.Operation));
			shader.SetInt(prefix + ".ColorBias", stage.ColorStage.Bias);
			shader.SetInt(prefix + ".ColorScale", stage.ColorStage.Scale);
			shader.SetInt(prefix + ".ColorClamp", stage.ColorStage.Clamp ? 1 : 0);
			shader.SetInt(prefix + ".ColorOutput", ToInt(stage.ColorStage.Output));

			shader.SetInt(prefix + ".AlphaA", ToInt(stage.AlphaStage.A));
			shader.SetInt(prefix + ".AlphaB", ToInt(stage.AlphaStage.B));
			shader.SetInt(prefix + ".AlphaC", ToInt(stage.AlphaStage.C));
			shader.SetInt(prefix + ".AlphaD", ToInt(stage.AlphaStage.D));
			shader.SetInt(prefix + ".AlphaOp", ToInt(stage.AlphaStage.Operation));
			shader.SetInt(prefix + ".AlphaBias", stage.AlphaStage.Bias);
			shader.SetInt(prefix + ".AlphaScale", stage.AlphaStage.Scale);
			shader.SetInt(prefix + ".AlphaClamp", stage.AlphaStage.Clamp ? 1 : 0);
			shader.SetInt(prefix + ".AlphaOutput", ToInt(stage.AlphaStage.Output));

			shader.SetInt(prefix + ".KonstColorSelector", stage.KonstColorSelector);
			shader.SetInt(prefix + ".KonstAlphaSelector", stage.KonstAlphaSelector);
		}
	}
}