#pragma once
#include <glad/glad.h>
#include <string>

class Texture {
public:
    enum Type : unsigned int 
    {
        TEXTURE_2D = 0,
        TEXTURE_3D
    };

    enum Format : unsigned int
    {
        RED = 1,
        RG,
        RGB,
        RGBA,
    };

    enum FormatType : unsigned int 
    {
        UNORM8 = 0,         
        UNORM16,            

        SNORM8,             
        SNORM16,            

        UINT8,              
        UINT16,             
        UINT32,             

        INT8,               
        INT16,              
        INT32,              

        FLOAT16,            
        FLOAT32             
    };

    Texture();
    Texture(const std::string& filename);
    Texture(int width, int height, unsigned char* data, Format format = RGBA,
        FormatType formatType = UNORM8);
    Texture(int width, int height, int depth, unsigned char* data, Format format = RGBA,
        FormatType formatType = UNORM8);
    ~Texture();

    bool LoadFromFile(const std::string& filename);
    bool CreateTexture2D(int width, int height, unsigned char* data,
        Format format = RGBA, FormatType formatType = UNORM8);
    bool CreateTexture3D(int width, int height, int depth, unsigned char* data,
        Format format = RGBA, FormatType formatType = UNORM8);

    bool CreateFloatTexture2D(int width, int height, float* data, Format format = RGBA);
    bool CreateFloatTexture3D(int width, int height, int depth, float* data, Format format = RGBA);

    void UpdateTexture2D(unsigned char* data, int xOffset = 0, int yOffset = 0,
        unsigned int width = 0, unsigned int height = 0);
    void UpdateTexture3D(unsigned char* data, int xOffset = 0, int yOffset = 0, int zOffset = 0,
        unsigned int width = 0, unsigned int height = 0, unsigned int depth = 0);

    void Bind(unsigned int unit = 0) const;
    void GenerateMipmaps();

    void SetHandle(GLuint handle) { m_textureID = handle; }

    unsigned int GetWidth() const { return m_width; }
    unsigned int GetHeight() const { return m_height; }
    unsigned int GetDepth() const { return m_depth; }
    Format GetFormat() const { return m_format; }
    FormatType GetFormatType() const { return m_formatType; }
    Type GetType() const { return m_type; }
    GLuint GetHandle() const { return m_textureID; }
    const std::string& GetFilename() const { return m_filename; }

    GLenum GetGLFormat() const;
    GLenum GetGLInternalFormat() const;
    GLenum GetGLDataType() const;

    bool IsIntegerFormat() const;
    bool IsNormalisedFormat() const;
    bool IsFloatFormat() const;

private:
    GLuint m_textureID;
    unsigned int m_width;
    unsigned int m_height;
    unsigned int m_depth;
    Format m_format;
    FormatType m_formatType;
    Type m_type;
    std::string m_filename;

    void SetPixelAlignment() const;
    void ResetPixelAlignment() const;
};