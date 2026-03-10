#include "cepch.h"
#include "ShaderStorageBuffer.h"
#include <glm/glm.hpp>

ShaderStorageBuffer::ShaderStorageBuffer()
{
	glGenBuffers(1, &m_ssboID);
}

ShaderStorageBuffer::~ShaderStorageBuffer()
{
	glDeleteBuffers(1, &m_ssboID);
}

void ShaderStorageBuffer::Bind()
{
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_ssboID);
}

void ShaderStorageBuffer::Unbind()
{
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void ShaderStorageBuffer::BindBufferBase(GLuint bindingIndex)
{
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, bindingIndex, m_ssboID);
}

void ShaderStorageBuffer::BufferData(GLsizeiptr size)
{
	glBufferData(GL_SHADER_STORAGE_BUFFER, size, NULL, GL_DYNAMIC_DRAW);
}

void ShaderStorageBuffer::BufferData(GLsizeiptr size, const void* data) {
	glBufferData(GL_SHADER_STORAGE_BUFFER, size, data, GL_DYNAMIC_DRAW);
}

void ShaderStorageBuffer::BufferSubData(GLintptr offset, GLsizeiptr size, const void* data)
{
	glBufferSubData(GL_SHADER_STORAGE_BUFFER, offset, size, data);
}

void ShaderStorageBuffer::ClearBufferData()
{
	const unsigned int zero = 0x00000000;
	glClearNamedBufferData(GL_SHADER_STORAGE_BUFFER, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, &zero);
}

void ShaderStorageBuffer::ClearBufferSubData(GLenum internalFormat, GLintptr offset, GLsizeiptr size, GLenum format, GLenum type, const void* data)
{
	glClearBufferSubData(GL_SHADER_STORAGE_BUFFER, internalFormat, offset, size, format, type, data);
}

void ShaderStorageBuffer::GetBufferSubData(GLintptr offset, GLsizeiptr size, void* data)
{
	glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, offset, size, data);
}

unsigned int ShaderStorageBuffer::GetBufferSize()
{
	int bytes;
	glGetBufferParameteriv(GL_SHADER_STORAGE_BUFFER, GL_BUFFER_SIZE, &bytes);
	return (unsigned int)bytes;
}

