#include "cepch.h"
#include "ShaderProgram.h"
#include "Core/Utilities/Serialiser.h"
#include <iostream>

namespace fs = std::filesystem;

ShaderProgram::ShaderProgram(std::string vertexFilename, std::string fragmentFilename)
	: m_vertexFilename(vertexFilename)
	, m_fragmentFilename(fragmentFilename)
	, m_vertexShader(0)
	, m_fragmentShader(0)
	, m_shaderProgram(0)
	, m_loadedSuccessfully(false)
{
	CompileAndLinkShaders();
}

bool ShaderProgram::CompileAndLinkShaders()
{
    m_vertexShader = glCreateShader(GL_VERTEX_SHADER);
    m_fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    m_shaderProgram = glCreateProgram();

    std::string vertexSource = CerealBox::LoadFileAsString(g_ContentPath + m_vertexFilename);
    std::string fragmentSource = CerealBox::LoadFileAsString(g_ContentPath + m_fragmentFilename);

    if (vertexSource.empty() || fragmentSource.empty())
    {
        std::cout << "Failed to open shader source files: " << m_vertexFilename
            << " or " << m_fragmentFilename << "\n";
        std::cout << "Is your working directory set up correctly?\n";
        return false;
    }

    std::unordered_set<std::string> includedFilesVert;
    std::unordered_set<std::string> includedFilesFrag;

    fs::path vertPath(g_ContentPath + m_vertexFilename);
    fs::path fragPath(g_ContentPath + m_fragmentFilename);

    std::string vertDir = vertPath.parent_path().string() + "/";
    std::string fragDir = fragPath.parent_path().string() + "/";

    vertexSource = ProcessIncludes(vertexSource, vertDir, includedFilesVert);
    fragmentSource = ProcessIncludes(fragmentSource, fragDir, includedFilesFrag);

    const char* vertexSourceC = vertexSource.c_str();
    glShaderSource(m_vertexShader, 1, &vertexSourceC, nullptr);
    glCompileShader(m_vertexShader);

    GLchar errorLog[512];
    GLint success = 0;
    glGetShaderiv(m_vertexShader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        std::string logText = "Vertex shader \"";
        logText.append(m_vertexFilename);
        logText.append("\" failed with error:");
        Logger::PrintLog(logText.c_str());
        glGetShaderInfoLog(m_vertexShader, 512, nullptr, errorLog);
        Logger::PrintLog(errorLog);

        glDeleteShader(m_vertexShader);
        glDeleteShader(m_fragmentShader);
        glDeleteProgram(m_shaderProgram);
        return false;
    }
    else
    {
        std::string logText = "Vertex shader \"";
        logText.append(m_vertexFilename);
        logText.append("\" compiled successfully!");
        Logger::PrintLog(logText.c_str());
    }

    const char* fragmentSourceC = fragmentSource.c_str();
    glShaderSource(m_fragmentShader, 1, &fragmentSourceC, nullptr);
    glCompileShader(m_fragmentShader);

    glGetShaderiv(m_fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        std::string logText = "Fragment shader \"";
        logText.append(m_fragmentFilename);
        logText.append("\" failed with error:");
        Logger::PrintLog(logText.c_str());

        glGetShaderInfoLog(m_fragmentShader, 512, nullptr, errorLog);
        Logger::PrintLog(errorLog);

        glDeleteShader(m_vertexShader);
        glDeleteShader(m_fragmentShader);
        glDeleteProgram(m_shaderProgram);
        return false;
    }
    else
    {
        std::string logText = "Fragment shader \"";
        logText.append(m_fragmentFilename);
        logText.append("\" compiled successfully!");
        Logger::PrintLog(logText.c_str());
    }

    glAttachShader(m_shaderProgram, m_vertexShader);
    glAttachShader(m_shaderProgram, m_fragmentShader);
    glLinkProgram(m_shaderProgram);

    glGetProgramiv(m_shaderProgram, GL_LINK_STATUS, &success);
    if (!success)
    {
        Logger::PrintLog("Error linking the shaders:");
        glGetProgramInfoLog(m_shaderProgram, 512, nullptr, errorLog);
        Logger::PrintLog(errorLog);

        glDeleteShader(m_vertexShader);
        glDeleteShader(m_fragmentShader);
        glDeleteProgram(m_shaderProgram);
        return false;
    }
    else
    {
        Logger::PrintLog("The shaders linked properly");
    }

    m_loadedSuccessfully = true;
    if (m_loadedSuccessfully)
    {
        IntrospectUniforms();
    }
    return true;
}

// New version
std::string ShaderProgram::ProcessIncludes(const std::string& source, const std::string& baseDir, std::unordered_set<std::string>& includedFiles, int depth)
{
    // Only added double slash comments for now so multi-line comments won't work.
    // (e.g. "/* This won't be processed as a comment. */")

    //const int MAX_INCLUDE_DEPTH = 32;
    if (depth > MAX_SHADER_INCLUDE_DEPTH)
    {
        Logger::PrintLog("ERROR: Maximum include depth exceeded (circular includes?)");
        return source;
    }

    std::istringstream stream(source);
    std::ostringstream result;
    std::string line;
    int lineNumber = 0;

    while (std::getline(stream, line))
    {
        // LineNumber is always being incremented in every possible branch that exits a loop so might as well place it at the front.
        lineNumber++;

        // Erase all characters in this line after "//"
        size_t commentPos = line.find("//");
        if (commentPos != std::string::npos)
            line.erase(commentPos, std::string::npos);

        // Check for existing include tag
        size_t includePos = line.find("#include");
        if (includePos == std::string::npos)
        {
            result << line << "\n";
            continue;
        }

        // Attempt to parse included file path.
        size_t firstQuote = line.find('"', includePos);
        size_t lastQuote = line.find('"', firstQuote + 1);
        if (firstQuote == std::string::npos || lastQuote == std::string::npos)
        {
            std::string warningMsg = "WARNING: Malformed #include at line ";
            warningMsg.append(std::to_string(lineNumber));
            Logger::PrintLog(warningMsg.c_str());
            continue;
        }

        std::string includePath = line.substr(firstQuote + 1, lastQuote - firstQuote - 1);
        std::string fullPath = baseDir + includePath;

        // Avoid circular includes.
        if (includedFiles.contains(fullPath))
        {
            result << "// Already included: " << includePath << "\n";
            continue;
        }
        includedFiles.insert(fullPath);

        // Attempt to load included file text.
        std::string includeSource = CerealBox::LoadFileAsString(fullPath);
        if (includeSource.empty())
        {
            std::string errorMsg = "WARNING: Failed to load include: ";
            errorMsg.append(includePath);
            Logger::PrintLog(errorMsg.c_str());
            continue;
        }

        // Successfully including file.
        result << "// Begin include: " << includePath << " (line " << lineNumber << ")\n";

        fs::path includedPath(fullPath);
        std::string includeDir = includedPath.parent_path().string() + "/";
        std::string processedInclude = ProcessIncludes(includeSource, includeDir, includedFiles, depth + 1);
        result << processedInclude << "\n";

        result << "// End include: " << includePath << "\n";
    }

    return result.str();
}

// Original
//std::string ShaderProgram::ProcessIncludes(
//    const std::string& source,
//    const std::string& baseDir,
//    std::unordered_set<std::string>& includedFiles,
//    int depth)
//{
//    const int MAX_INCLUDE_DEPTH = 32;
//    if (depth > MAX_INCLUDE_DEPTH)
//    {
//        Logger::PrintLog("ERROR: Maximum include depth exceeded (circular includes?)");
//        return source;
//    }
//
//    std::istringstream stream(source);
//    std::ostringstream result;
//    std::string line;
//    int lineNumber = 1;
//
//    while (std::getline(stream, line))
//    {
//        size_t includePos = line.find("#include");
//
//        if (includePos != std::string::npos)
//        {
//            size_t firstQuote = line.find('"', includePos);
//            size_t lastQuote = line.find('"', firstQuote + 1);
//
//            if (firstQuote != std::string::npos && lastQuote != std::string::npos)
//            {
//                std::string includePath = line.substr(firstQuote + 1, lastQuote - firstQuote - 1);
//
//                std::string fullPath = baseDir + includePath;
//
//                if (includedFiles.find(fullPath) != includedFiles.end())
//                {
//                    result << "// Already included: " << includePath << "\n";
//                    lineNumber++;
//                    continue;
//                }
//
//                includedFiles.insert(fullPath);
//
//                std::string includeSource = CerealBox::LoadFileAsString(fullPath);
//
//                if (!includeSource.empty())
//                {
//                    result << "// Begin include: " << includePath << " (line " << lineNumber << ")\n";
//
//                    fs::path includedPath(fullPath);
//                    std::string includeDir = includedPath.parent_path().string() + "/";
//
//                    std::string processedInclude = ProcessIncludes(
//                        includeSource,
//                        includeDir,
//                        includedFiles,
//                        depth + 1
//                    );
//
//                    result << processedInclude;
//                    result << "// End include: " << includePath << "\n";
//                }
//                else
//                {
//                    std::string errorMsg = "WARNING: Failed to load include: ";
//                    errorMsg.append(includePath);
//                    Logger::PrintLog(errorMsg.c_str());
//                    result << line << "\n"; 
//                }
//            }
//            else
//            {
//                std::string warningMsg = "WARNING: Malformed #include at line ";
//                warningMsg.append(std::to_string(lineNumber));
//                Logger::PrintLog(warningMsg.c_str());
//                result << line << "\n";
//            }
//        }
//        else
//        {
//            result << line << "\n";
//        }
//
//        lineNumber++;
//    }
//
//    return result.str();
//}

ShaderProgram::~ShaderProgram()
{
	if (m_loadedSuccessfully)
	{
		glDeleteShader(m_vertexShader);
		glDeleteShader(m_fragmentShader);
		glDeleteProgram(m_shaderProgram);
	}
}

ShaderProgram::ShaderProgram(ShaderProgram&& other) noexcept
{
	this->m_shaderProgram = other.m_shaderProgram;
	this->m_fragmentShader = other.m_fragmentShader;
	this->m_vertexShader = other.m_vertexShader;
	this->m_loadedSuccessfully = other.m_loadedSuccessfully;

	other.m_loadedSuccessfully = false;
}

ShaderProgram& ShaderProgram::operator=(ShaderProgram&& other) noexcept
{
	if (&other == this)
	{
		return *this;
	}
	if (m_loadedSuccessfully)
	{
		glDeleteShader(m_vertexShader);
		glDeleteShader(m_fragmentShader);
		glDeleteProgram(m_shaderProgram);
	}
	this->m_shaderProgram = other.m_shaderProgram;
	this->m_fragmentShader = other.m_fragmentShader;
	this->m_vertexShader = other.m_vertexShader;
	this->m_loadedSuccessfully = other.m_loadedSuccessfully;

	other.m_loadedSuccessfully = false;	
	return *this;
}

void ShaderProgram::Use() const
{
	glUseProgram(m_shaderProgram);
}

bool ShaderProgram::Reload()
{
	if (m_loadedSuccessfully)
	{
		glDetachShader(m_shaderProgram, m_vertexShader);
		glDetachShader(m_shaderProgram, m_fragmentShader);

		glDeleteShader(m_vertexShader);
		glDeleteShader(m_fragmentShader);
		glDeleteProgram(m_shaderProgram);

		glFlush();
		glFinish();

		m_vertexShader = 0;
		m_fragmentShader = 0;
		m_shaderProgram = 0;
		m_loadedSuccessfully = false;
	}

	bool success = CompileAndLinkShaders();

	return success;
}

void ShaderProgram::IntrospectUniforms()
{
    if (!m_loadedSuccessfully) return;

    m_exposedUniforms.clear();
    m_uniformLocationCache.clear();

    GLint uniformCount;
    glGetProgramiv(m_shaderProgram, GL_ACTIVE_UNIFORMS, &uniformCount);

    for (GLint i = 0; i < uniformCount; ++i)
    {
        GLchar name[256];
        GLsizei length;
        GLint size;
        GLenum type;

        glGetActiveUniform(m_shaderProgram, i, sizeof(name), &length, &size, &type, name);

        std::string uniformName(name);

        if (IsPrivateUniform(uniformName)) continue;

        UniformInfo info;
        info.name = uniformName;
        info.type = GLTypeToUniformType(type);
        info.location = glGetUniformLocation(m_shaderProgram, name);

        m_exposedUniforms.push_back(info);

        m_uniformLocationCache[uniformName] = info.location;
    }
}

UniformType ShaderProgram::GLTypeToUniformType(GLenum glType) const
{
	switch (glType)
	{
	case GL_FLOAT: return UniformType::Float;
	case GL_INT: return UniformType::Int;
	case GL_BOOL: return UniformType::Bool;
	case GL_FLOAT_VEC2: return UniformType::Vec2;
	case GL_FLOAT_VEC3: return UniformType::Vec3;
	case GL_FLOAT_VEC4: return UniformType::Vec4;
	case GL_SAMPLER_2D: return UniformType::Sampler2D;
	default: return UniformType::Float;
	}
}

bool ShaderProgram::IsPrivateUniform(const std::string& name) const
{
	return name != "albedoMap" &&
        name != "normalMap" &&
        name != "roughMap" &&
        name != "metalMap" &&
        name != "emissiveMap" &&
        name != "colourTint" &&
      //  name != "emissiveIntensity" &&
        name != "tiling" &&
        name != "textureScale" &&
        name != "blendSharpness";
}

GLint ShaderProgram::GetUniformLocation(const std::string& name) const
{
    auto it = m_uniformLocationCache.find(name);
    if (it != m_uniformLocationCache.end())
    {
        return it->second;
    }

    GLint location = glGetUniformLocation(m_shaderProgram, name.c_str());
    m_uniformLocationCache[name] = location;
    return location;
}

void ShaderProgram::SetBoolUniform(const std::string& varName, bool value) const
{
    GLint varLoc = GetUniformLocation(varName);
    if (varLoc != -1)
        glUniform1i(varLoc, (int)value);
}

void ShaderProgram::SetIntUniform(const std::string& varName, int value) const
{
    GLint varLoc = GetUniformLocation(varName);
    if (varLoc != -1)
        glUniform1i(varLoc, value);
}

void ShaderProgram::SetUintUniform(const std::string& varName, unsigned int value) const
{
    GLint varLoc = GetUniformLocation(varName);
    if (varLoc != -1)
        glUniform1ui(varLoc, value);
}

void ShaderProgram::SetFloatUniform(const std::string varName, float value) const
{
    GLint varLoc = GetUniformLocation(varName);
    if (varLoc != -1)
        glUniform1f(varLoc, value);
}

void ShaderProgram::SetVec2Uniform(const std::string& varName, const glm::vec2& value) const
{
    GLint varLoc = GetUniformLocation(varName);
    if (varLoc != -1)
        glUniform2f(varLoc, value.x, value.y);
}

void ShaderProgram::SetVec3Uniform(const std::string& varName, const glm::vec3& value) const
{
    GLint varLoc = GetUniformLocation(varName);
    if (varLoc != -1)
        glUniform3f(varLoc, value.x, value.y, value.z);
}

void ShaderProgram::SetVec4Uniform(const std::string& varName, const glm::vec4& value) const
{
    GLint varLoc = GetUniformLocation(varName);
    if (varLoc != -1)
        glUniform4f(varLoc, value.x, value.y, value.z, value.w);
}

void ShaderProgram::SetIVec3Uniform(const std::string& varName, const glm::ivec3& value) const
{
    GLint varLoc = GetUniformLocation(varName);
    if (varLoc != -1)
        glUniform3iv(varLoc, 1, &value[0]);
}

void ShaderProgram::SetMat4Uniform(const std::string varName, glm::mat4 value) const
{
    GLint varLoc = GetUniformLocation(varName);
    if (varLoc != -1)
        glUniformMatrix4fv(varLoc, 1, GL_FALSE, &value[0][0]);
}