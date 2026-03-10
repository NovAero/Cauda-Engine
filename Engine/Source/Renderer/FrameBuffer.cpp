#include "cepch.h"
#include "FrameBuffer.h"
#include <iostream>
#include <glm/glm.hpp>

FrameBuffer::FrameBuffer(int width, int height, int depth)
    : m_width(width)
    , m_height(height)
    , m_depth(depth)
    , m_framebufferID(0)
    , m_hasDepth(false)
    , m_depthIsStencil(false)
    , m_depthHighPrecision(false)
{
    glGenFramebuffers(1, &m_framebufferID);
}

FrameBuffer::~FrameBuffer()
{
    if (m_framebufferID != 0)
    {
        glDeleteFramebuffers(1, &m_framebufferID);
    }
}

void FrameBuffer::Bind()
{
    glBindFramebuffer(GL_FRAMEBUFFER, m_framebufferID);
    glViewport(0, 0, m_width, m_height);
}

void FrameBuffer::Unbind()
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

int FrameBuffer::AddColourAttachment(Texture::Format format, Texture::FormatType formatType)
{
    Bind();

    int attachmentIndex = static_cast<int>(m_colourAttachments.size());
    auto texture = std::make_unique<Texture>();

    texture->CreateTexture2D(m_width, m_height, nullptr, format, formatType);

    glBindTexture(GL_TEXTURE_2D, texture->GetHandle());
    if (formatType == Texture::INT8)
    {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }
    else
    {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + attachmentIndex,
        GL_TEXTURE_2D, texture->GetHandle(), 0);

    m_attachmentInfo.push_back({ format, formatType, false, false, FilterMode::NEAREST });
    m_colourAttachments.push_back(std::move(texture));

    UpdateDrawBuffers();
    Unbind();

    return attachmentIndex;
}

int FrameBuffer::Add3DColourAttachment(Texture::Format format, Texture::FormatType formatType)
{
    if (m_depth <= 1)
    {
        std::cerr << "Cannot add 3D texture to a 2D framebuffer. Use depth > 1 in constructor." << std::endl;
        return -1;
    }

    Bind();

    int attachmentIndex = static_cast<int>(m_colourAttachments.size());
    auto texture = std::make_unique<Texture>();

    texture->CreateTexture3D(m_width, m_height, m_depth, nullptr, format, formatType);

    glBindTexture(GL_TEXTURE_3D, texture->GetHandle());
    if (formatType == Texture::INT8)
    {
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }
    else
    {
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + attachmentIndex,
        texture->GetHandle(), 0);

    m_attachmentInfo.push_back({ format, formatType, true, false, FilterMode::NEAREST });
    m_colourAttachments.push_back(std::move(texture));

    UpdateDrawBuffers();
    Unbind();

    return attachmentIndex;
}

int FrameBuffer::AddFloatColourAttachment(Texture::Format format, FilterMode filterMode)
{
    Bind();

    int attachmentIndex = static_cast<int>(m_colourAttachments.size());
    auto texture = std::make_unique<Texture>();

    texture->CreateFloatTexture2D(m_width, m_height, nullptr, format);

    glBindTexture(GL_TEXTURE_2D, texture->GetHandle());

    GLenum filter = (filterMode == FilterMode::LINEAR) ? GL_LINEAR : GL_NEAREST;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + attachmentIndex,
        GL_TEXTURE_2D, texture->GetHandle(), 0);

    m_attachmentInfo.push_back({ format, Texture::FLOAT16, false, true, filterMode });
    m_colourAttachments.push_back(std::move(texture));

    UpdateDrawBuffers();
    Unbind();

    return attachmentIndex;
}

int FrameBuffer::AddFloat3DColourAttachment(Texture::Format format)
{
    if (m_depth <= 1)
    {
        std::cerr << "Cannot add 3D texture to a 2D framebuffer. Use depth > 1 in constructor." << std::endl;
        return -1;
    }

    Bind();

    int attachmentIndex = static_cast<int>(m_colourAttachments.size());
    auto texture = std::make_unique<Texture>();

    texture->CreateFloatTexture3D(m_width, m_height, m_depth, nullptr, format);

    glBindTexture(GL_TEXTURE_3D, texture->GetHandle());
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + attachmentIndex,
        texture->GetHandle(), 0);

    m_attachmentInfo.push_back({ format, Texture::FLOAT16, true, true, FilterMode::NEAREST });
    m_colourAttachments.push_back(std::move(texture));

    UpdateDrawBuffers();
    Unbind();

    return attachmentIndex;
}

bool FrameBuffer::AddDepthAttachment(bool highPrecision)
{
    if (m_depthAttachment)
    {
        std::cerr << "Framebuffer already has a depth attachment" << std::endl;
        return false;
    }

    Bind();

    m_depthAttachment = std::make_unique<Texture>();
    GLuint depthHandle;
    glGenTextures(1, &depthHandle);

    GLenum internalFormat = highPrecision ? GL_DEPTH_COMPONENT32F : GL_DEPTH_COMPONENT24;
    GLenum type = highPrecision ? GL_FLOAT : GL_UNSIGNED_INT;

    if (m_depth > 1)
    {
        glBindTexture(GL_TEXTURE_3D, depthHandle);
        glTexImage3D(GL_TEXTURE_3D, 0, internalFormat, m_width, m_height, m_depth, 0,
            GL_DEPTH_COMPONENT, type, nullptr);

        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

        glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depthHandle, 0);
    }
    else
    {
        glBindTexture(GL_TEXTURE_2D, depthHandle);
        glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, m_width, m_height, 0,
            GL_DEPTH_COMPONENT, type, nullptr);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
            GL_TEXTURE_2D, depthHandle, 0);
    }

    m_depthAttachment->SetHandle(depthHandle);
    m_hasDepth = true;
    m_depthIsStencil = false;
    m_depthHighPrecision = highPrecision;

    Unbind();
    return true;
}

bool FrameBuffer::AddDepthStencilAttachment()
{
    if (m_depthAttachment)
    {
        std::cerr << "Framebuffer already has a depth attachment" << std::endl;
        return false;
    }

    Bind();

    m_depthAttachment = std::make_unique<Texture>();
    GLuint depthHandle;
    glGenTextures(1, &depthHandle);

    if (m_depth > 1)
    {
        glBindTexture(GL_TEXTURE_3D, depthHandle);
        glTexImage3D(GL_TEXTURE_3D, 0, GL_DEPTH24_STENCIL8, m_width, m_height, m_depth, 0,
            GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, nullptr);

        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

        glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, depthHandle, 0);
    }
    else
    {
        glBindTexture(GL_TEXTURE_2D, depthHandle);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, m_width, m_height, 0,
            GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, nullptr);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
            GL_TEXTURE_2D, depthHandle, 0);
    }

    m_depthAttachment->SetHandle(depthHandle);
    m_hasDepth = true;
    m_depthIsStencil = true;
    m_depthHighPrecision = false;

    Unbind();
    return true;
}

void FrameBuffer::SetAttachmentName(int index, const std::string& name)
{
    if (name.empty())
    {
        return;
    }

    if (index >= 0 && index < m_colourAttachments.size())
    {
        m_namedAttachments[name] = m_colourAttachments[index].get();
    }
    else if (index == -1 && m_depthAttachment)
    {
        m_namedAttachments[name] = m_depthAttachment.get();
    }
    else
    {
        std::cerr << "Invalid attachment index: " << index << std::endl;
    }
}

void FrameBuffer::Resize(int width, int height, int depth)
{
    if (width == m_width && height == m_height && depth == m_depth)
    {
        return;
    }

    m_width = width;
    m_height = height;
    m_depth = depth;

    std::vector<AttachmentInfo> attachmentInfoCopy = m_attachmentInfo;
    bool hadDepth = m_hasDepth;
    bool depthWasStencil = m_depthIsStencil;
    bool depthWasHighPrecision = m_depthHighPrecision;

    m_colourAttachments.clear();
    m_depthAttachment.reset();
    m_namedAttachments.clear();
    m_attachmentInfo.clear();
    m_hasDepth = false;

    for (const auto& info : attachmentInfoCopy)
    {
        if (info.isFloat)
        {
            if (info.is3D && depth > 1)
            {
                AddFloat3DColourAttachment(info.format);
            }
            else
            {
                AddFloatColourAttachment(info.format, info.filterMode); 
            }
        }
        else
        {
            if (info.is3D && depth > 1)
            {
                Add3DColourAttachment(info.format, info.formatType);
            }
            else
            {
                AddColourAttachment(info.format, info.formatType);
            }
        }
    }

    if (hadDepth)
    {
        if (depthWasStencil)
        {
            AddDepthStencilAttachment();
        }
        else
        {
            AddDepthAttachment(depthWasHighPrecision);
        }
    }
}

void FrameBuffer::Clear(const glm::vec4& clearColor)
{
    Bind();
    glClearColor(clearColor.r, clearColor.g, clearColor.b, clearColor.a);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    Unbind();
}

const Texture& FrameBuffer::GetColourAttachment(int index) const
{
    if (index < 0 || index >= m_colourAttachments.size())
    {
        throw std::out_of_range("Color attachment index out of range");
    }

    return *m_colourAttachments[index];
}

const Texture& FrameBuffer::GetDepthAttachment() const
{
    if (!m_depthAttachment)
    {
        throw std::runtime_error("No depth attachment");
    }

    return *m_depthAttachment;
}

const Texture& FrameBuffer::GetAttachmentByName(const std::string& name) const
{
    auto it = m_namedAttachments.find(name);
    if (it == m_namedAttachments.end())
    {
        throw std::runtime_error("No attachment with name: " + name);
    }

    return *(it->second);
}

const std::vector<std::unique_ptr<Texture>>& FrameBuffer::GetColourAttachments() const
{
    return m_colourAttachments;
}

bool FrameBuffer::IsComplete() const
{
    glBindFramebuffer(GL_FRAMEBUFFER, m_framebufferID);
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

    if (status != GL_FRAMEBUFFER_COMPLETE)
    {
        std::cerr << "Framebuffer not complete: ";
        switch (status)
        {
        case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
            std::cerr << "Incomplete attachment" << std::endl;
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
            std::cerr << "Missing attachment" << std::endl;
            break;
        case GL_FRAMEBUFFER_UNSUPPORTED:
            std::cerr << "Unsupported format combination" << std::endl;
            break;
        default:
            std::cerr << "Unknown error: " << status << std::endl;
            break;
        }
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return status == GL_FRAMEBUFFER_COMPLETE;
}

void FrameBuffer::SetDrawBuffers(const std::vector<int>& indices)
{
    Bind();

    if (indices.empty())
    {
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
    }
    else
    {
        std::vector<GLenum> drawBuffers;
        drawBuffers.reserve(indices.size());

        for (int index : indices)
        {
            drawBuffers.push_back(GL_COLOR_ATTACHMENT0 + index);
        }

        glDrawBuffers(static_cast<GLsizei>(drawBuffers.size()), drawBuffers.data());
    }

    Unbind();
}

void FrameBuffer::UpdateDrawBuffers()
{
    if (m_colourAttachments.empty())
    {
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
        return;
    }

    std::vector<GLenum> drawBuffers;
    drawBuffers.reserve(m_colourAttachments.size());

    for (size_t i = 0; i < m_colourAttachments.size(); ++i)
    {
        drawBuffers.push_back(GL_COLOR_ATTACHMENT0 + static_cast<GLenum>(i));
    }

    glDrawBuffers(static_cast<GLsizei>(drawBuffers.size()), drawBuffers.data());
}