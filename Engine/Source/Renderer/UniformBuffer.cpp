#include "cepch.h"
#include "UniformBuffer.h"
#include <glm/glm.hpp>

UniformBuffer::UniformBuffer()
{
	glGenBuffers(1, &m_uboID);
}

UniformBuffer::~UniformBuffer()
{
	glDeleteBuffers(1, &m_uboID);
}

void UniformBuffer::Bind()
{
	glBindBuffer(GL_UNIFORM_BUFFER, m_uboID);
}

void UniformBuffer::Unbind()
{
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

void UniformBuffer::BindBufferBase(GLuint bindingIndex)
{
	glBindBufferBase(GL_UNIFORM_BUFFER, bindingIndex, m_uboID);
}

void UniformBuffer::BufferData(GLsizeiptr size)
{
	glBufferData(GL_UNIFORM_BUFFER, size, NULL, GL_STATIC_DRAW);
}

void UniformBuffer::BufferData(GLsizeiptr size, const void* data)
{
	glBufferData(GL_UNIFORM_BUFFER, size, data, GL_STATIC_DRAW);
}

void UniformBuffer::BufferSubData(GLintptr offset, GLsizeiptr size, const void* data)
{
	glBufferSubData(GL_UNIFORM_BUFFER, offset, size, data);
}

void UniformBuffer::ClearBufferSubData(GLenum internalFormat, GLintptr offset, GLsizeiptr size, GLenum format, GLenum type, const void* data)
{
	glClearBufferSubData(GL_UNIFORM_BUFFER, internalFormat, offset, size, format, type, data);
}

void UniformBuffer::GetBufferSubData(GLintptr offset, GLsizeiptr size, void* data)
{
	glGetBufferSubData(GL_UNIFORM_BUFFER, offset, size, data);
}

unsigned int UniformBuffer::GetBufferSize()
{
	int bytes;
	glGetBufferParameteriv(GL_UNIFORM_BUFFER, GL_BUFFER_SIZE, &bytes);
	return (unsigned int)bytes;
}
