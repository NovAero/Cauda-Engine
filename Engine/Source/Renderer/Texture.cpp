#include "cepch.h"
#include "Texture.h"
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <iostream>
#include <algorithm>

Texture::Texture()
    : m_textureID(0), m_width(0), m_height(0), m_depth(1),
    m_format(RGBA), m_formatType(UNORM8), m_type(TEXTURE_2D)
{
}

Texture::Texture(const std::string& filename)
    : Texture()
{
    LoadFromFile(filename);
}

Texture::Texture(int width, int height, unsigned char* data, Format format, FormatType formatType)
    : Texture()
{
    CreateTexture2D(width, height, data, format, formatType);
}

Texture::Texture(int width, int height, int depth, unsigned char* data, Format format, FormatType formatType)
    : Texture()
{
    CreateTexture3D(width, height, depth, data, format, formatType);
}

Texture::~Texture()
{
    if (m_textureID != 0)
    {
        glDeleteTextures(1, &m_textureID);
    }
}

bool Texture::LoadFromFile(const std::string& filename)
{
    if (m_textureID != 0)
    {
        glDeleteTextures(1, &m_textureID);
        m_textureID = 0;
    }

    std::string fullPath = g_ContentPath + filename;
    int channels;
    stbi_set_flip_vertically_on_load(false);
    unsigned char* data = stbi_load(fullPath.c_str(),
        (int*)&m_width, (int*)&m_height, &channels, STBI_default);

    if (stbi_is_hdr(filename.c_str()))
    {
        float* hdrData = stbi_loadf(filename.c_str(),
            (int*)&m_width, (int*)&m_height, &channels, STBI_default);

        if (!hdrData)
        {
            std::cerr << "Failed to load HDR texture: " << filename << std::endl;
            return false;
        }

        switch (channels)
        {
        case 1: m_format = RED;   break;
        case 2: m_format = RG;    break;
        case 3: m_format = RGB;   break;
        case 4: m_format = RGBA;  break;
        default:
            std::cerr << "Unsupported number of channels: " << channels << std::endl;
            stbi_image_free(hdrData);
            return false;
        }

        m_formatType = FLOAT16;
        m_type = TEXTURE_2D;
        m_depth = 1;

        bool result = CreateFloatTexture2D(m_width, m_height, hdrData, m_format);
        stbi_image_free(hdrData);
        return result;
    }
    else
    {
        std::string fullPath = g_ContentPath + filename;

        unsigned char* data = stbi_load(fullPath.c_str(),
            (int*)&m_width, (int*)&m_height, &channels, STBI_default);

        if (!data)
        {
            std::cerr << "Failed to load texture: " << filename << std::endl;
            return false;
        }

        switch (channels)
        {
        case 1: m_format = RED;   break;
        case 2: m_format = RG;    break;
        case 3: m_format = RGB;   break;
        case 4: m_format = RGBA;  break;
        default:
            std::cerr << "Unsupported number of channels: " << channels << std::endl;
            stbi_image_free(data);
            return false;
        }

        m_formatType = UNORM8;

        bool result = CreateTexture2D(m_width, m_height, data, m_format, m_formatType);
        stbi_image_free(data);
        return result;
    }
}

bool Texture::CreateTexture2D(int width, int height, unsigned char* data, Format format, FormatType formatType)
{
    if (m_textureID != 0)
    {
        glDeleteTextures(1, &m_textureID);
    }

    m_width = width;
    m_height = height;
    m_depth = 1;
    m_format = format;
    m_formatType = formatType;
    m_type = TEXTURE_2D;

    glGenTextures(1, &m_textureID);
    glBindTexture(GL_TEXTURE_2D, m_textureID);

    if (IsIntegerFormat())
    {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }
    else
    {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    SetPixelAlignment();

    glTexImage2D(GL_TEXTURE_2D, 0, GetGLInternalFormat(), width, height,
        0, GetGLFormat(), GL_UNSIGNED_BYTE, data);

    if (!IsIntegerFormat())
    {
        glGenerateMipmap(GL_TEXTURE_2D);
    }

    ResetPixelAlignment();

    return true;
}

bool Texture::CreateTexture3D(int width, int height, int depth, unsigned char* data, Format format, FormatType formatType)
{
    if (m_textureID != 0)
    {
        glDeleteTextures(1, &m_textureID);
    }

    m_width = width;
    m_height = height;
    m_depth = depth;
    m_format = format;
    m_formatType = formatType;
    m_type = TEXTURE_3D;

    glGenTextures(1, &m_textureID);
    glBindTexture(GL_TEXTURE_3D, m_textureID);

    if (IsIntegerFormat())
    {
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }
    else
    {
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }

    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_REPEAT);

    SetPixelAlignment();

    glTexImage3D(GL_TEXTURE_3D, 0, GetGLInternalFormat(), width, height, depth,
        0, GetGLFormat(), GL_UNSIGNED_BYTE, data);

    if (!IsIntegerFormat())
    {
        glGenerateMipmap(GL_TEXTURE_3D);
    }

    ResetPixelAlignment();

    return true;
}

bool Texture::CreateFloatTexture2D(int width, int height, float* data, Format format)
{
    if (m_textureID != 0)
    {
        glDeleteTextures(1, &m_textureID);
    }

    m_width = width;
    m_height = height;
    m_depth = 1;
    m_format = format;
    m_formatType = FLOAT16;  
    m_type = TEXTURE_2D;

    glGenTextures(1, &m_textureID);
    glBindTexture(GL_TEXTURE_2D, m_textureID);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glTexImage2D(GL_TEXTURE_2D, 0, GetGLInternalFormat(), width, height,
        0, GetGLFormat(), GL_FLOAT, data);

    glGenerateMipmap(GL_TEXTURE_2D);

    return true;
}

bool Texture::CreateFloatTexture3D(int width, int height, int depth, float* data, Format format)
{
    if (m_textureID != 0)
    {
        glDeleteTextures(1, &m_textureID);
    }

    m_width = width;
    m_height = height;
    m_depth = depth;
    m_format = format;
    m_formatType = FLOAT16;
    m_type = TEXTURE_3D;

    glGenTextures(1, &m_textureID);
    glBindTexture(GL_TEXTURE_3D, m_textureID);

    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_REPEAT);

    glTexImage3D(GL_TEXTURE_3D, 0, GetGLInternalFormat(), width, height, depth,
        0, GetGLFormat(), GL_FLOAT, data);

    glGenerateMipmap(GL_TEXTURE_3D);

    return true;
}

void Texture::UpdateTexture2D(unsigned char* data, int xOffset, int yOffset,
    unsigned int width, unsigned int height)
{
    if (m_textureID == 0 || m_type != TEXTURE_2D)
    {
        std::cerr << "Cannot update: Not a valid 2D texture" << std::endl;
        return;
    }

    if (width == 0) width = m_width - xOffset;
    if (height == 0) height = m_height - yOffset;

    if (xOffset < 0 || yOffset < 0 ||
        xOffset + width > m_width || yOffset + height > m_height)
    {
        std::cerr << "Invalid update dimensions or offsets" << std::endl;
        return;
    }

    glBindTexture(GL_TEXTURE_2D, m_textureID);

    SetPixelAlignment();

    glTexSubImage2D(GL_TEXTURE_2D, 0, xOffset, yOffset, width, height,
        GetGLFormat(), GL_UNSIGNED_BYTE, data);

    ResetPixelAlignment();

    if (m_formatType != INT8)
    {
        glGenerateMipmap(GL_TEXTURE_2D);
    }
}

void Texture::UpdateTexture3D(unsigned char* data, int xOffset, int yOffset, int zOffset,
    unsigned int width, unsigned int height, unsigned int depth)
{
    if (m_textureID == 0 || m_type != TEXTURE_3D)
    {
        std::cerr << "Cannot update: Not a valid 3D texture" << std::endl;
        return;
    }

    if (width == 0) width = m_width - xOffset;
    if (height == 0) height = m_height - yOffset;
    if (depth == 0) depth = m_depth - zOffset;

    if (xOffset < 0 || yOffset < 0 || zOffset < 0 ||
        xOffset + width > m_width ||
        yOffset + height > m_height ||
        zOffset + depth > m_depth)
    {
        std::cerr << "Invalid update dimensions or offsets" << std::endl;
        return;
    }

    glBindTexture(GL_TEXTURE_3D, m_textureID);

    SetPixelAlignment();

    glTexSubImage3D(GL_TEXTURE_3D, 0, xOffset, yOffset, zOffset,
        width, height, depth, GetGLFormat(), GL_UNSIGNED_BYTE, data);

    ResetPixelAlignment();

    if (m_formatType != INT8)
    {
        glGenerateMipmap(GL_TEXTURE_3D);
    }
}

void Texture::Bind(unsigned int unit) const
{
    if (m_textureID == 0)
    {
        return;
    }

    glActiveTexture(GL_TEXTURE0 + unit);

    if (m_type == TEXTURE_3D)
    {
        glBindTexture(GL_TEXTURE_3D, m_textureID);
    }
    else
    {
        glBindTexture(GL_TEXTURE_2D, m_textureID);
    }
}

void Texture::GenerateMipmaps()
{
    if (m_textureID == 0 || m_formatType == INT8)
    {
        return;
    }

    if (m_type == TEXTURE_3D)
    {
        glBindTexture(GL_TEXTURE_3D, m_textureID);
        glGenerateMipmap(GL_TEXTURE_3D);
    }
    else
    {
        glBindTexture(GL_TEXTURE_2D, m_textureID);
        glGenerateMipmap(GL_TEXTURE_2D);
    }
}

GLenum Texture::GetGLFormat() const
{
    bool isInteger = (m_formatType >= UINT8 && m_formatType <= INT32);

    switch (m_format)
    {
    case RED:
        return isInteger ? GL_RED_INTEGER : GL_RED;
    case RG:
        return isInteger ? GL_RG_INTEGER : GL_RG;
    case RGB:
        return isInteger ? GL_RGB_INTEGER : GL_RGB;
    case RGBA:
    default:
        return isInteger ? GL_RGBA_INTEGER : GL_RGBA;
    }
}

GLenum Texture::GetGLInternalFormat() const
{
    switch (m_format)
    {
    case RED:
        switch (m_formatType)
        {
        case UNORM8:    return GL_R8;
        case UNORM16:   return GL_R16;
        case SNORM8:    return GL_R8_SNORM;
        case SNORM16:   return GL_R16_SNORM;

        case UINT8:     return GL_R8UI;
        case UINT16:    return GL_R16UI;
        case UINT32:    return GL_R32UI;    
        case INT8:      return GL_R8I;
        case INT16:     return GL_R16I;
        case INT32:     return GL_R32I;

        case FLOAT16:   return GL_R16F;
        case FLOAT32:   return GL_R32F;

        default:        return GL_R8;
        }
        break;

    case RG:
        switch (m_formatType)
        {
        case UNORM8:    return GL_RG8;
        case UNORM16:   return GL_RG16;
        case SNORM8:    return GL_RG8_SNORM;
        case SNORM16:   return GL_RG16_SNORM;

        case UINT8:     return GL_RG8UI;
        case UINT16:    return GL_RG16UI;
        case UINT32:    return GL_RG32UI;
        case INT8:      return GL_RG8I;
        case INT16:     return GL_RG16I;
        case INT32:     return GL_RG32I;

        case FLOAT16:   return GL_RG16F;
        case FLOAT32:   return GL_RG32F;

        default:        return GL_RG8;
        }
        break;

    case RGB:
        switch (m_formatType)
        {
        case UNORM8:    return GL_RGB8;
        case UNORM16:   return GL_RGB16;
        case SNORM8:    return GL_RGB8_SNORM;
        case SNORM16:   return GL_RGB16_SNORM;

        case UINT8:     return GL_RGB8UI;
        case UINT16:    return GL_RGB16UI;
        case UINT32:    return GL_RGB32UI;
        case INT8:      return GL_RGB8I;
        case INT16:     return GL_RGB16I;
        case INT32:     return GL_RGB32I;

        case FLOAT16:   return GL_RGB16F;
        case FLOAT32:   return GL_RGB32F;

        default:        return GL_RGB8;
        }
        break;

    case RGBA:
    default:
        switch (m_formatType)
        {
        case UNORM8:    return GL_RGBA8;
        case UNORM16:   return GL_RGBA16;
        case SNORM8:    return GL_RGBA8_SNORM;
        case SNORM16:   return GL_RGBA16_SNORM;

        case UINT8:     return GL_RGBA8UI;
        case UINT16:    return GL_RGBA16UI;
        case UINT32:    return GL_RGBA32UI;
        case INT8:      return GL_RGBA8I;
        case INT16:     return GL_RGBA16I;
        case INT32:     return GL_RGBA32I;

        case FLOAT16:   return GL_RGBA16F;
        case FLOAT32:   return GL_RGBA32F;

        default:        return GL_RGBA8;
        }
        break;
    }
}

GLenum Texture::GetGLDataType() const
{
    switch (m_formatType)
    {
    case UNORM8:    return GL_UNSIGNED_BYTE;
    case UNORM16:   return GL_UNSIGNED_SHORT;
    case SNORM8:    return GL_BYTE;
    case SNORM16:   return GL_SHORT;

    case UINT8:     return GL_UNSIGNED_BYTE;
    case UINT16:    return GL_UNSIGNED_SHORT;
    case UINT32:    return GL_UNSIGNED_INT;
    case INT8:      return GL_BYTE;
    case INT16:     return GL_SHORT;
    case INT32:     return GL_INT;

    case FLOAT16:   return GL_HALF_FLOAT;
    case FLOAT32:   return GL_FLOAT;

    default:        return GL_UNSIGNED_BYTE;
    }
}

bool Texture::IsIntegerFormat() const
{
    return m_formatType >= UINT8 && m_formatType <= INT32;
}

bool Texture::IsNormalisedFormat() const
{
    return m_formatType >= UNORM8 && m_formatType <= SNORM16;
}

bool Texture::IsFloatFormat() const
{
    return m_formatType == FLOAT16 || m_formatType == FLOAT32;
}

void Texture::SetPixelAlignment() const
{
    GLint alignment = 4;  

    switch (m_format)
    {
    case RED:
        alignment = 1; 
        break;
    case RG:
        alignment = 2; 
        break;
    case RGB:
        alignment = 1;  
        break;
    case RGBA:
        alignment = 4;  
        break;
    }

    glPixelStorei(GL_UNPACK_ALIGNMENT, alignment);
}

void Texture::ResetPixelAlignment() const
{
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
}