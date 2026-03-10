#include "cepch.h"
#include "ComputeShader.h"
#include "Core/Utilities/Serialiser.h"
#include <iostream>


namespace fs = std::filesystem;

ComputeShader::ComputeShader(std::string computeFilename)
    : m_filename(computeFilename)
    , m_computeShader(0)
    , m_shaderProgram(0)
    , m_loadedSuccessfully(false)
{
    CompileAndLinkShader();
}

ComputeShader::~ComputeShader() {
    if (m_loadedSuccessfully) {
        glDeleteShader(m_computeShader);
        glDeleteProgram(m_shaderProgram);
    }
}

ComputeShader::ComputeShader(ComputeShader&& other) noexcept {
    this->m_shaderProgram = other.m_shaderProgram;
    this->m_computeShader = other.m_computeShader;
    this->m_loadedSuccessfully = other.m_loadedSuccessfully;
    this->m_filename = std::move(other.m_filename);

    other.m_loadedSuccessfully = false;
}

ComputeShader& ComputeShader::operator=(ComputeShader&& other) noexcept {
    if (&other == this) {
        return *this;
    }
    if (m_loadedSuccessfully) {
        glDeleteShader(m_computeShader);
        glDeleteProgram(m_shaderProgram);
    }
    this->m_shaderProgram = other.m_shaderProgram;
    this->m_computeShader = other.m_computeShader;
    this->m_loadedSuccessfully = other.m_loadedSuccessfully;
    this->m_filename = std::move(other.m_filename);

    other.m_loadedSuccessfully = false;
    return *this;
}

bool ComputeShader::CompileAndLinkShader() {
    m_computeShader = glCreateShader(GL_COMPUTE_SHADER);
    m_shaderProgram = glCreateProgram();

    std::string computeSource = CerealBox::LoadFileAsString(g_ContentPath + m_filename);
    if (computeSource.empty()) {
        std::cout << "Failed to open compute shader source file: " << m_filename << "\n";
        std::cout << "Is your working directory set up correctly?\n";
        return false;
    }

    std::unordered_set<std::string> includedFiles;

    fs::path computePath(g_ContentPath + m_filename);
    std::string computeDir = computePath.parent_path().string() + "/";
    computeSource = ProcessIncludes(computeSource, computeDir, includedFiles);

    const char* computeSourceC = computeSource.c_str();
    glShaderSource(m_computeShader, 1, &computeSourceC, nullptr);
    glCompileShader(m_computeShader);

    GLchar errorLog[512];
    GLint success = 0;
    glGetShaderiv(m_computeShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        
        std::string logText = "Compute shader \"";
        logText.append(m_filename);
        logText.append("\" failed with error:");
        Logger::PrintLog(logText.c_str());
        glGetShaderInfoLog(m_computeShader, 512, nullptr, errorLog);
        Logger::PrintLog(errorLog);

        glDeleteShader(m_computeShader);
        glDeleteProgram(m_shaderProgram);
        return false;
    }
    else {
        std::string logText = "Compute shader \"";
        logText.append(m_filename);
        logText.append("\" compiled successfully!");
        Logger::PrintLog(logText.c_str());
    }

    glAttachShader(m_shaderProgram, m_computeShader);
    glLinkProgram(m_shaderProgram);

    glGetProgramiv(m_shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        std::cout << "Error linking the compute shader.\n";
        glGetProgramInfoLog(m_shaderProgram, 512, nullptr, errorLog);
        std::cout << errorLog << '\n';

        glDeleteShader(m_computeShader);
        glDeleteProgram(m_shaderProgram);
        return false;
    }
    else {
        std::cout << "The compute shader linked properly\n";
    }

    m_loadedSuccessfully = true;
    return true;
}

void ComputeShader::Use() const {
    glUseProgram(m_shaderProgram);
}

bool ComputeShader::Reload() {
    if (m_loadedSuccessfully) {
        glDetachShader(m_shaderProgram, m_computeShader);
        glDeleteShader(m_computeShader);
        glDeleteProgram(m_shaderProgram);

        glFlush();
        glFinish();

        m_computeShader = 0;
        m_shaderProgram = 0;
        m_loadedSuccessfully = false;
    }

    bool success = CompileAndLinkShader();
    if (success) Use();

    return success;
}

void ComputeShader::Dispatch(GLuint numGroupsX, GLuint numGroupsY, GLuint numGroupsZ) const {
    glDispatchCompute(numGroupsX, numGroupsY, numGroupsZ);
}

void ComputeShader::DispatchWithDimensions(GLuint width, GLuint height, GLuint depth,
    GLuint localSizeX, GLuint localSizeY, GLuint localSizeZ) const {
    GLuint numGroupsX = (width + localSizeX - 1) / localSizeX;
    GLuint numGroupsY = (height + localSizeY - 1) / localSizeY;
    GLuint numGroupsZ = (depth + localSizeZ - 1) / localSizeZ;

    glDispatchCompute(numGroupsX, numGroupsY, numGroupsZ);
}

void ComputeShader::Synchronize(GLbitfield barriers) const {
    glMemoryBarrier(barriers);
}

void ComputeShader::SetBoolUniform(const std::string& varName, bool value) const {
    GLint varLoc = glGetUniformLocation(m_shaderProgram, varName.c_str());
    glUniform1i(varLoc, (int)value);
}

void ComputeShader::SetIntUniform(const std::string& varName, int value) const {
    GLint varLoc = glGetUniformLocation(m_shaderProgram, varName.c_str());
    glUniform1i(varLoc, value);
}

void ComputeShader::SetUintUniform(const std::string& varName, unsigned int value) const {
    GLint varLoc = glGetUniformLocation(m_shaderProgram, varName.c_str());
    glUniform1ui(varLoc, value);
}

void ComputeShader::SetFloatUniform(const std::string& varName, float value) const {
    GLint varLoc = glGetUniformLocation(m_shaderProgram, varName.c_str());
    glUniform1f(varLoc, value);
}

void ComputeShader::SetVec2Uniform(const std::string& varName, const glm::vec2& value) const {
    GLint varLoc = glGetUniformLocation(m_shaderProgram, varName.c_str());
    glUniform2f(varLoc, value.x, value.y);
}

void ComputeShader::SetVec3Uniform(const std::string& varName, const glm::vec3& value) const {
    GLint varLoc = glGetUniformLocation(m_shaderProgram, varName.c_str());
    glUniform3f(varLoc, value.x, value.y, value.z);
}

void ComputeShader::SetVec4Uniform(const std::string& varName, const glm::vec4& value) const {
    GLint varLoc = glGetUniformLocation(m_shaderProgram, varName.c_str());
    glUniform4f(varLoc, value.x, value.y, value.z, value.w);
}

void ComputeShader::SetIVec3Uniform(const std::string& varName, const glm::ivec3& value) const {
    GLint varLoc = glGetUniformLocation(m_shaderProgram, varName.c_str());
    glUniform3i(varLoc, value.x, value.y, value.z);
}

void ComputeShader::SetMat4Uniform(const std::string& varName, const glm::mat4& value) const {
    GLint varLoc = glGetUniformLocation(m_shaderProgram, varName.c_str());
    glUniformMatrix4fv(varLoc, 1, GL_FALSE, &value[0][0]);
}

void ComputeShader::BindImage(GLuint textureId, GLuint unit, GLint level, GLboolean layered,
    GLint layer, GLenum access, GLenum format) const {
    glBindImageTexture(unit, textureId, level, layered, layer, access, format);
}

void ComputeShader::BindImage3D(GLuint textureId, GLuint unit, GLint level, GLenum access, GLenum format) const {
    glBindImageTexture(unit, textureId, level, GL_TRUE, 0, access, format);
}

GLuint ComputeShader::GetProgramId() const {
    return m_shaderProgram;
}

bool ComputeShader::IsLoaded() const {
    return m_loadedSuccessfully;
}


std::string ComputeShader::ProcessIncludes(const std::string& source, const std::string& baseDir, std::unordered_set<std::string>& includedFiles, int depth)
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
