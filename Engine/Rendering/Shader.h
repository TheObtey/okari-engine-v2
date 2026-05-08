#pragma once

#include <glm.hpp>
#include <string>

namespace Okari
{
	class Shader
	{
	public:
		Shader(const std::string& vertexPath, const std::string& fragmentPath);
		~Shader();

		void Bind() const;
		void Unbind() const;
		void SetMat4(const std::string& name, const glm::mat4& value) const;
		void SetInt(const std::string& name, int value) const;
		void SetUInt(const std::string& name, unsigned int value) const;
		void SetFloat(const std::string& name, float value) const;
		void SetVec3(const std::string& name, const glm::vec3& value) const;
		void SetVec4(const std::string& name, const glm::vec4& value) const;

	private:
		unsigned int CompileShader(unsigned int type, const std::string& source);
		std::string ReadFile(const std::string& path);

	private:
		unsigned int m_RendererID = 0;
	};
}