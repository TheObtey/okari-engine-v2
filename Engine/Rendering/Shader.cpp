#include "Rendering/Shader.h"
#include <glad/glad.h>
#include <gtc/type_ptr.hpp>
#include <fstream>
#include <sstream>
#include <iostream>

namespace Okari
{
    Shader::Shader(const std::string& vertexPath, const std::string& fragmentPath)
    {
        std::string vertexSource = ReadFile(vertexPath);
        std::string fragmentSource = ReadFile(fragmentPath);

        unsigned int vertexShader = CompileShader(GL_VERTEX_SHADER, vertexSource);
        unsigned int fragmentShader = CompileShader(GL_FRAGMENT_SHADER, fragmentSource);

        m_RendererID = glCreateProgram();

        glAttachShader(m_RendererID, vertexShader);
        glAttachShader(m_RendererID, fragmentShader);
        glLinkProgram(m_RendererID);

        int success;
        glGetProgramiv(m_RendererID, GL_LINK_STATUS, &success);

        if (!success)
        {
            char infoLog[1024];
            glGetProgramInfoLog(m_RendererID, 1024, nullptr, infoLog);
            std::cerr << "Shader link error:\n" << infoLog << std::endl;
        }

        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
    }

    Shader::~Shader()
    {
        glDeleteProgram(m_RendererID);
    }

    void Shader::Bind() const
    {
        glUseProgram(m_RendererID);
    }

    void Shader::Unbind() const
    {
        glUseProgram(0);
    }

    void Shader::SetMat4(const std::string& name, const glm::mat4& value) const
    {
        int location = glGetUniformLocation(m_RendererID, name.c_str());

        if (location == -1)
        {
            std::cerr << "Uniform not found: " << name << std::endl;
            return;
        }

        glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(value));
    }

    void Shader::SetInt(const std::string& name, int value) const
    {
        int location = glGetUniformLocation(m_RendererID, name.c_str());

        if (location == -1)
        {
            std::cerr << "Uniform not found: " << name << std::endl;
            return;
        }

        glUniform1i(location, value);
    }

    unsigned int Shader::CompileShader(unsigned int type, const std::string& source)
    {
        unsigned int shader = glCreateShader(type);

        const char* src = source.c_str();
        glShaderSource(shader, 1, &src, nullptr);
        glCompileShader(shader);

        int success;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

        if (!success)
        {
            char infoLog[1024];
            glGetShaderInfoLog(shader, 1024, nullptr, infoLog);
            std::cerr << "Shader compile error:\n" << infoLog << std::endl;
        }

        return shader;
    }

    std::string Shader::ReadFile(const std::string& path)
    {
        std::ifstream file(path);

        if (!file.is_open())
        {
            std::cerr << "Failed to open shader file: " << path << std::endl;
            return "";
        }

        std::stringstream buffer;
        buffer << file.rdbuf();

        return buffer.str();
    }
}