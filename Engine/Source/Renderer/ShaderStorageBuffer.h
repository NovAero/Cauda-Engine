#pragma once
#include <glad/glad.h>

class ShaderStorageBuffer
{
private:
	GLuint m_ssboID;

public:
	ShaderStorageBuffer();
	~ShaderStorageBuffer();

	void Bind();
	void Unbind();

	void BindBufferBase(GLuint bindingIndex);

	void BufferData(GLsizeiptr size);
	void BufferData(GLsizeiptr size, const void* data);
	void BufferSubData(GLintptr offset, GLsizeiptr size, const void* data);
	void ClearBufferData();
	void ClearBufferSubData(GLenum internalFormat, GLintptr offset, GLsizeiptr size, GLenum format, GLenum type, const void* data);
	void GetBufferSubData(GLintptr offset, GLsizeiptr  size, void* data);

	unsigned int GetBufferSize();
};