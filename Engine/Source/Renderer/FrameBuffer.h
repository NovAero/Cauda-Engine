#pragma once
#include <glad/glad.h>
#include <memory>
#include <vector>
#include <string>
#include <unordered_map>
#include "Texture.h"


class FrameBuffer
{
public:
    enum class FilterMode
    {
        NEAREST,
        LINEAR
    };

    FrameBuffer(int width = 1920, int height = 1080, int depth = 1);
    ~FrameBuffer();

    void Bind();
    void Unbind();

    int AddColourAttachment(Texture::Format format = Texture::RGBA,
        Texture::FormatType formatType = Texture::UNORM8);
    int Add3DColourAttachment(Texture::Format format = Texture::RGBA,
        Texture::FormatType formatType = Texture::UNORM8);

    int AddFloatColourAttachment(Texture::Format format = Texture::RGBA,
        FilterMode filterMode = FilterMode::NEAREST);
    int AddFloat3DColourAttachment(Texture::Format format = Texture::RGBA);

    bool AddDepthAttachment(bool highPrecision = false);
    bool AddDepthStencilAttachment();

    void SetAttachmentName(int index, const std::string& name);
    void Resize(int width, int height, int depth = 1);
    void Clear(const glm::vec4& clearColor = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));

    const Texture& GetColourAttachment(int index = 0) const;
    const Texture& GetDepthAttachment() const;
    const Texture& GetAttachmentByName(const std::string& name) const;
    const std::vector<std::unique_ptr<Texture>>& GetColourAttachments() const;

    bool IsComplete() const;
    void SetDrawBuffers(const std::vector<int>& indices);

    int GetWidth() const { return m_width; }
    int GetHeight() const { return m_height; }
    int GetDepth() const { return m_depth; }
    GLuint GetHandle() const { return m_framebufferID; }

private:
    GLuint m_framebufferID;
    int m_width;
    int m_height;
    int m_depth;

    std::vector<std::unique_ptr<Texture>> m_colourAttachments;
    std::unique_ptr<Texture> m_depthAttachment;
    std::unordered_map<std::string, Texture*> m_namedAttachments;

    struct AttachmentInfo 
    {
        Texture::Format format;
        Texture::FormatType formatType;
        bool is3D;
        bool isFloat;  
        FilterMode filterMode;
    };
    std::vector<AttachmentInfo> m_attachmentInfo;
    bool m_hasDepth;
    bool m_depthIsStencil;
    bool m_depthHighPrecision;

    void UpdateDrawBuffers();
};