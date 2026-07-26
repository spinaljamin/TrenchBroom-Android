/*
 Copyright (C) 2026 Kristian Duske

 This file is part of TrenchBroom.

 TrenchBroom is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 TrenchBroom is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with TrenchBroom. If not, see <http://www.gnu.org/licenses/>.
 */

#include "ui/GlQt.h"

#include <array>
#include <vector>

#if !defined(Q_OS_ANDROID)
#include <QOpenGLFunctions_2_1>
#endif

namespace tb::ui
{

GlQt::GlQt(OpenGLFunctions& gl)
  : m_gl{gl}
{
}

#if defined(Q_OS_ANDROID)
void GlQt::uploadAndroidMatrixUniforms()
{
  auto currentProgram = GLint{0};
  m_gl.glGetIntegerv(GL_CURRENT_PROGRAM, &currentProgram);
  if (currentProgram == 0)
  {
    return;
  }

  const auto projectionLocation =
    m_gl.glGetUniformLocation(GLuint(currentProgram), "tb_ProjectionMatrix");
  if (projectionLocation != -1)
  {
    m_gl.glUniformMatrix4fv(projectionLocation, 1, GL_FALSE, m_projectionMatrix.data());
  }

  const auto modelViewLocation =
    m_gl.glGetUniformLocation(GLuint(currentProgram), "tb_ModelViewMatrix");
  if (modelViewLocation != -1)
  {
    m_gl.glUniformMatrix4fv(modelViewLocation, 1, GL_FALSE, m_modelViewMatrix.data());
  }

  const auto normalLocation =
    m_gl.glGetUniformLocation(GLuint(currentProgram), "tb_NormalMatrix");
  if (normalLocation != -1)
  {
    const auto normalMatrix = std::array<GLfloat, 9>{
      m_modelViewMatrix[0], m_modelViewMatrix[1], m_modelViewMatrix[2],
      m_modelViewMatrix[4], m_modelViewMatrix[5], m_modelViewMatrix[6],
      m_modelViewMatrix[8], m_modelViewMatrix[9], m_modelViewMatrix[10]};
    m_gl.glUniformMatrix3fv(normalLocation, 1, GL_FALSE, normalMatrix.data());
  }
}
#endif

void GlQt::clear(const GLbitfield mask)
{
  m_gl.glClear(mask);
}

void GlQt::clearColor(
  const GLfloat red, const GLfloat green, const GLfloat blue, const GLfloat alpha)
{
  m_gl.glClearColor(red, green, blue, alpha);
}

void GlQt::viewport(
  const GLint x, const GLint y, const GLsizei width, const GLsizei height)
{
  m_gl.glViewport(x, y, width, height);
}

void GlQt::matrixMode(const GLenum mode)
{
#if !defined(Q_OS_ANDROID)
  m_gl.glMatrixMode(mode);
#else
  m_matrixMode = mode;
#endif
}

void GlQt::loadMatrixd(const GLdouble* matrix)
{
#if !defined(Q_OS_ANDROID)
  m_gl.glLoadMatrixd(matrix);
#else
  auto floatMatrix = std::array<GLfloat, 16>{};
  for (auto i = size_t{0}; i < floatMatrix.size(); ++i)
  {
    floatMatrix[i] = GLfloat(matrix[i]);
  }
  loadMatrixf(floatMatrix.data());
#endif
}

void GlQt::loadMatrixf(const GLfloat* matrix)
{
#if !defined(Q_OS_ANDROID)
  m_gl.glLoadMatrixf(matrix);
#else
  auto& target = m_matrixMode == GL_PROJECTION ? m_projectionMatrix : m_modelViewMatrix;
  std::copy(matrix, matrix + target.size(), target.begin());
  uploadAndroidMatrixUniforms();
#endif
}

void GlQt::getBooleanv(const GLenum pname, GLboolean* params)
{
  m_gl.glGetBooleanv(pname, params);
}

void GlQt::getDoublev(const GLenum pname, GLdouble* params)
{
  #if !defined(Q_OS_ANDROID)
  m_gl.glGetDoublev(pname, params);
#else
  auto values = std::array<GLfloat, 16>{};
  m_gl.glGetFloatv(pname, values.data());
  for (auto i = size_t{0}; i < values.size(); ++i)
  {
    params[i] = GLdouble(values[i]);
  }
#endif
}

void GlQt::getFloatv(const GLenum pname, GLfloat* params)
{
  m_gl.glGetFloatv(pname, params);
}

void GlQt::getIntegerv(const GLenum pname, GLint* params)
{
  m_gl.glGetIntegerv(pname, params);
}

void GlQt::enableClientState(const GLenum cap)
{
#if !defined(Q_OS_ANDROID)
  m_gl.glEnableClientState(cap);
#else
  switch (cap)
  {
  case GL_VERTEX_ARRAY:
    m_gl.glEnableVertexAttribArray(0);
    break;
  case GL_NORMAL_ARRAY:
    m_gl.glEnableVertexAttribArray(1);
    break;
  case GL_COLOR_ARRAY:
    m_gl.glEnableVertexAttribArray(2);
    break;
  case GL_TEXTURE_COORD_ARRAY:
    m_gl.glEnableVertexAttribArray(3);
    break;
  default:
    break;
  }
#endif
}

void GlQt::disableClientState(const GLenum cap)
{
#if !defined(Q_OS_ANDROID)
  m_gl.glDisableClientState(cap);
#else
  switch (cap)
  {
  case GL_VERTEX_ARRAY:
    m_gl.glDisableVertexAttribArray(0);
    break;
  case GL_NORMAL_ARRAY:
    m_gl.glDisableVertexAttribArray(1);
    break;
  case GL_COLOR_ARRAY:
    m_gl.glDisableVertexAttribArray(2);
    break;
  case GL_TEXTURE_COORD_ARRAY:
    m_gl.glDisableVertexAttribArray(3);
    break;
  default:
    break;
  }
#endif
}

void GlQt::pushAttrib(const GLbitfield mask)
{
  #if !defined(Q_OS_ANDROID)
  m_gl.glPushAttrib(mask);
#else
  static_cast<void>(mask);
#endif
}

void GlQt::popAttrib()
{
  #if !defined(Q_OS_ANDROID)
  m_gl.glPopAttrib();
#endif
}

void GlQt::enable(const GLenum cap)
{
#if defined(Q_OS_ANDROID)
  if (cap == GL_TEXTURE_2D)
  {
    return;
  }
#endif
  m_gl.glEnable(cap);
}

void GlQt::disable(const GLenum cap)
{
#if defined(Q_OS_ANDROID)
  if (cap == GL_TEXTURE_2D)
  {
    return;
  }
#endif
  m_gl.glDisable(cap);
}

void GlQt::lineWidth(const GLfloat width)
{
  m_gl.glLineWidth(width);
}

void GlQt::polygonMode(const GLenum face, const GLenum mode)
{
  #if !defined(Q_OS_ANDROID)
  m_gl.glPolygonMode(face, mode);
#else
  static_cast<void>(face);
  static_cast<void>(mode);
#endif
}

void GlQt::frontFace(const GLenum mode)
{
  m_gl.glFrontFace(mode);
}

void GlQt::cullFace(const GLenum mode)
{
  m_gl.glCullFace(mode);
}

void GlQt::blendFunc(const GLenum sfactor, const GLenum dfactor)
{
  m_gl.glBlendFunc(sfactor, dfactor);
}

void GlQt::shadeModel(const GLenum mode)
{
  #if !defined(Q_OS_ANDROID)
  m_gl.glShadeModel(mode);
#else
  static_cast<void>(mode);
#endif
}

void GlQt::depthMask(const GLboolean flag)
{
  m_gl.glDepthMask(flag);
}

void GlQt::depthRange(const GLclampd nearVal, const GLclampd farVal)
{
  #if defined(Q_OS_ANDROID)
  m_gl.glDepthRangef(GLfloat(nearVal), GLfloat(farVal));
#else
  m_gl.glDepthRange(nearVal, farVal);
#endif
}

void GlQt::depthFunc(const GLenum func)
{
  m_gl.glDepthFunc(func);
}

GLuint GlQt::createProgram()
{
  return m_gl.glCreateProgram();
}

void GlQt::deleteProgram(const GLuint program)
{
  m_gl.glDeleteProgram(program);
}

void GlQt::linkProgram(const GLuint program)
{
  m_gl.glLinkProgram(program);
}

void GlQt::getProgramInfoLog(
  const GLuint program, const GLsizei maxLength, GLsizei* length, GLchar* infoLog)
{
  m_gl.glGetProgramInfoLog(program, maxLength, length, infoLog);
}

void GlQt::getProgramiv(const GLuint program, const GLenum pname, GLint* params)
{
  m_gl.glGetProgramiv(program, pname, params);
}

void GlQt::useProgram(const GLuint program)
{
  m_gl.glUseProgram(program);
#if defined(Q_OS_ANDROID)
  uploadAndroidMatrixUniforms();
#endif
}

GLuint GlQt::createShader(const GLenum shaderType)
{
  return m_gl.glCreateShader(shaderType);
}

void GlQt::deleteShader(const GLuint shader)
{
  m_gl.glDeleteShader(shader);
}

void GlQt::attachShader(const GLuint program, const GLuint shader)
{
  m_gl.glAttachShader(program, shader);
}

void GlQt::shaderSource(
  const GLuint shader,
  const GLsizei count,
  const GLchar* const* string,
  const GLint* length)
{
  #if defined(Q_OS_ANDROID)
  m_gl.glShaderSource(shader, count, const_cast<const char**>(string), length);
#else
  m_gl.glShaderSource(shader, count, string, length);
#endif
}

void GlQt::compileShader(const GLuint shader)
{
  m_gl.glCompileShader(shader);
}

void GlQt::getShaderInfoLog(
  const GLuint shader, const GLsizei maxLength, GLsizei* length, GLchar* infoLog)
{
  m_gl.glGetShaderInfoLog(shader, maxLength, length, infoLog);
}

void GlQt::getShaderiv(const GLuint shader, const GLenum pname, GLint* params)
{
  m_gl.glGetShaderiv(shader, pname, params);
}

void GlQt::uniform1f(const GLint location, const GLfloat v0)
{
  m_gl.glUniform1f(location, v0);
}

void GlQt::uniform2f(const GLint location, const GLfloat v0, const GLfloat v1)
{
  m_gl.glUniform2f(location, v0, v1);
}

void GlQt::uniform3f(
  const GLint location, const GLfloat v0, const GLfloat v1, const GLfloat v2)
{
  m_gl.glUniform3f(location, v0, v1, v2);
}

void GlQt::uniform4f(
  const GLint location,
  const GLfloat v0,
  const GLfloat v1,
  const GLfloat v2,
  const GLfloat v3)
{
  m_gl.glUniform4f(location, v0, v1, v2, v3);
}

void GlQt::uniform1i(const GLint location, const GLint v0)
{
  m_gl.glUniform1i(location, v0);
}

void GlQt::uniform2i(const GLint location, const GLint v0, const GLint v1)
{
  m_gl.glUniform2i(location, v0, v1);
}

void GlQt::uniform3i(const GLint location, const GLint v0, const GLint v1, const GLint v2)
{
  m_gl.glUniform3i(location, v0, v1, v2);
}

void GlQt::uniform4i(
  const GLint location, const GLint v0, const GLint v1, const GLint v2, const GLint v3)
{
  m_gl.glUniform4i(location, v0, v1, v2, v3);
}

void GlQt::uniform1fv(const GLint location, const GLsizei count, const GLfloat* value)
{
  m_gl.glUniform1fv(location, count, value);
}

void GlQt::uniform2fv(const GLint location, const GLsizei count, const GLfloat* value)
{
  m_gl.glUniform2fv(location, count, value);
}

void GlQt::uniform3fv(const GLint location, const GLsizei count, const GLfloat* value)
{
  m_gl.glUniform3fv(location, count, value);
}

void GlQt::uniform4fv(const GLint location, const GLsizei count, const GLfloat* value)
{
  m_gl.glUniform4fv(location, count, value);
}

void GlQt::uniform1iv(const GLint location, const GLsizei count, const GLint* value)
{
  m_gl.glUniform1iv(location, count, value);
}

void GlQt::uniform2iv(const GLint location, const GLsizei count, const GLint* value)
{
  m_gl.glUniform2iv(location, count, value);
}

void GlQt::uniform3iv(const GLint location, const GLsizei count, const GLint* value)
{
  m_gl.glUniform3iv(location, count, value);
}

void GlQt::uniform4iv(const GLint location, const GLsizei count, const GLint* value)
{
  m_gl.glUniform4iv(location, count, value);
}

void GlQt::uniformMatrix2fv(
  const GLint location,
  const GLsizei count,
  const GLboolean transpose,
  const GLfloat* value)
{
  m_gl.glUniformMatrix2fv(location, count, transpose, value);
}

void GlQt::uniformMatrix3fv(
  const GLint location,
  const GLsizei count,
  const GLboolean transpose,
  const GLfloat* value)
{
  m_gl.glUniformMatrix3fv(location, count, transpose, value);
}

void GlQt::uniformMatrix4fv(
  const GLint location,
  const GLsizei count,
  const GLboolean transpose,
  const GLfloat* value)
{
  m_gl.glUniformMatrix4fv(location, count, transpose, value);
}

void GlQt::uniformMatrix2x3fv(
  const GLint location,
  const GLsizei count,
  const GLboolean transpose,
  const GLfloat* value)
{
  #if !defined(Q_OS_ANDROID)
  m_gl.glUniformMatrix2x3fv(location, count, transpose, value);
#else
  static_cast<void>(location); static_cast<void>(count); static_cast<void>(transpose); static_cast<void>(value);
#endif
}

void GlQt::uniformMatrix3x2fv(
  const GLint location,
  const GLsizei count,
  const GLboolean transpose,
  const GLfloat* value)
{
  #if !defined(Q_OS_ANDROID)
  m_gl.glUniformMatrix3x2fv(location, count, transpose, value);
#else
  static_cast<void>(location); static_cast<void>(count); static_cast<void>(transpose); static_cast<void>(value);
#endif
}

void GlQt::uniformMatrix2x4fv(
  const GLint location,
  const GLsizei count,
  const GLboolean transpose,
  const GLfloat* value)
{
  #if !defined(Q_OS_ANDROID)
  m_gl.glUniformMatrix2x4fv(location, count, transpose, value);
#else
  static_cast<void>(location); static_cast<void>(count); static_cast<void>(transpose); static_cast<void>(value);
#endif
}

void GlQt::uniformMatrix4x2fv(
  const GLint location,
  const GLsizei count,
  const GLboolean transpose,
  const GLfloat* value)
{
  #if !defined(Q_OS_ANDROID)
  m_gl.glUniformMatrix4x2fv(location, count, transpose, value);
#else
  static_cast<void>(location); static_cast<void>(count); static_cast<void>(transpose); static_cast<void>(value);
#endif
}

void GlQt::uniformMatrix3x4fv(
  const GLint location,
  const GLsizei count,
  const GLboolean transpose,
  const GLfloat* value)
{
  #if !defined(Q_OS_ANDROID)
  m_gl.glUniformMatrix3x4fv(location, count, transpose, value);
#else
  static_cast<void>(location); static_cast<void>(count); static_cast<void>(transpose); static_cast<void>(value);
#endif
}

void GlQt::uniformMatrix4x3fv(
  const GLint location,
  const GLsizei count,
  const GLboolean transpose,
  const GLfloat* value)
{
  #if !defined(Q_OS_ANDROID)
  m_gl.glUniformMatrix4x3fv(location, count, transpose, value);
#else
  static_cast<void>(location); static_cast<void>(count); static_cast<void>(transpose); static_cast<void>(value);
#endif
}

GLint GlQt::getAttribLocation(const GLuint program, const GLchar* name)
{
  return m_gl.glGetAttribLocation(program, name);
}

GLint GlQt::getUniformLocation(const GLuint program, const GLchar* name)
{
  return m_gl.glGetUniformLocation(program, name);
}

void GlQt::genBuffers(const GLsizei n, GLuint* buffers)
{
  m_gl.glGenBuffers(n, buffers);
}

void GlQt::deleteBuffers(const GLsizei n, const GLuint* buffers)
{
  m_gl.glDeleteBuffers(n, buffers);
}

void GlQt::bindBuffer(const GLenum target, const GLuint buffer)
{
  m_gl.glBindBuffer(target, buffer);
}

void GlQt::bufferData(
  const GLenum target, const GLsizeiptr size, const GLvoid* data, const GLenum usage)
{
  m_gl.glBufferData(target, size, data, usage);
}

void GlQt::bufferSubData(
  const GLenum target, const GLintptr offset, const GLsizeiptr size, const void* data)
{
  m_gl.glBufferSubData(target, offset, size, data);
}

void GlQt::vertexPointer(
  const GLint size, const GLenum type, const GLsizei stride, const GLvoid* ptr)
{
#if !defined(Q_OS_ANDROID)
  m_gl.glVertexPointer(size, type, stride, ptr);
#else
  m_gl.glVertexAttribPointer(0, size, type, GL_FALSE, stride, ptr);
#endif
}

void GlQt::colorPointer(
  const GLint size, const GLenum type, const GLsizei stride, const GLvoid* pointer)
{
#if !defined(Q_OS_ANDROID)
  m_gl.glColorPointer(size, type, stride, pointer);
#else
  m_gl.glVertexAttribPointer(2, size, type, GL_FALSE, stride, pointer);
#endif
}

void GlQt::normalPointer(const GLenum type, const GLsizei stride, const GLvoid* ptr)
{
#if !defined(Q_OS_ANDROID)
  m_gl.glNormalPointer(type, stride, ptr);
#else
  m_gl.glVertexAttribPointer(1, 3, type, GL_FALSE, stride, ptr);
#endif
}

void GlQt::texCoordPointer(
  const GLint size, const GLenum type, const GLsizei stride, const GLvoid* pointer)
{
#if !defined(Q_OS_ANDROID)
  m_gl.glTexCoordPointer(size, type, stride, pointer);
#else
  m_gl.glVertexAttribPointer(3, size, type, GL_FALSE, stride, pointer);
#endif
}

void GlQt::enableVertexAttribArray(const GLuint index)
{
  m_gl.glEnableVertexAttribArray(index);
}

void GlQt::disableVertexAttribArray(const GLuint index)
{
  m_gl.glDisableVertexAttribArray(index);
}

void GlQt::vertexAttribPointer(
  const GLuint index,
  const GLint size,
  const GLenum type,
  const GLboolean normalized,
  const GLsizei stride,
  const void* pointer)
{
  m_gl.glVertexAttribPointer(index, size, type, normalized, stride, pointer);
}

void GlQt::genTextures(const GLsizei n, GLuint* textures)
{
  m_gl.glGenTextures(n, textures);
}

void GlQt::deleteTextures(const GLsizei n, const GLuint* textures)
{
  m_gl.glDeleteTextures(n, textures);
}

void GlQt::bindTexture(const GLenum target, const GLuint texture)
{
  m_gl.glBindTexture(target, texture);
}

void GlQt::activeTexture(const GLenum texture)
{
  m_gl.glActiveTexture(texture);
}

void GlQt::texImage2D(
  const GLenum target,
  const GLint level,
  const GLint internalFormat,
  const GLsizei width,
  const GLsizei height,
  const GLint border,
  const GLenum format,
  const GLenum type,
  const GLvoid* data)
{
#if defined(Q_OS_ANDROID)
  const auto androidInternalFormat = internalFormat == GL_RGBA ? format : internalFormat;
  m_gl.glTexImage2D(
    target, level, androidInternalFormat, width, height, border, format, type, data);
#else
  m_gl.glTexImage2D(
    target, level, internalFormat, width, height, border, format, type, data);
#endif
}

void GlQt::compressedTexImage2D(
  const GLenum target,
  const GLint level,
  const GLenum internalformat,
  const GLsizei width,
  const GLsizei height,
  const GLint border,
  const GLsizei imageSize,
  const GLvoid* data)
{
  m_gl.glCompressedTexImage2D(
    target, level, internalformat, width, height, border, imageSize, data);
}

void GlQt::texParameterf(const GLenum target, const GLenum pname, const GLfloat param)
{
  m_gl.glTexParameterf(target, pname, param);
}

void GlQt::texParameteri(const GLenum target, const GLenum pname, const GLint param)
{
  m_gl.glTexParameteri(target, pname, param);
}

void GlQt::pixelStoref(const GLenum pname, const GLfloat param)
{
  #if defined(Q_OS_ANDROID)
  m_gl.glPixelStorei(pname, GLint(param));
#else
  m_gl.glPixelStoref(pname, param);
#endif
}

void GlQt::pixelStorei(const GLenum pname, const GLint param)
{
#if defined(Q_OS_ANDROID)
  if (pname == GL_UNPACK_SWAP_BYTES || pname == GL_UNPACK_LSB_FIRST)
  {
    return;
  }
#endif
  m_gl.glPixelStorei(pname, param);
}

void GlQt::clientActiveTexture(const GLenum texture)
{
  #if !defined(Q_OS_ANDROID)
  m_gl.glClientActiveTexture(texture);
#else
  static_cast<void>(texture);
#endif
}

void GlQt::drawArrays(const GLenum mode, const GLint first, const GLsizei count)
{
#if defined(Q_OS_ANDROID)
  if (mode == GL_QUADS)
  {
    for (auto offset = GLint{0}; offset + 3 < count; offset += 4)
    {
      m_gl.glDrawArrays(GL_TRIANGLE_FAN, first + offset, 4);
    }
    return;
  }
#endif
  m_gl.glDrawArrays(mode, first, count);
}

void GlQt::drawElements(
  const GLenum mode, const GLsizei count, const GLenum type, const void* indices)
{
#if defined(Q_OS_ANDROID)
  if (mode == GL_QUADS)
  {
    const auto indexSize = type == GL_UNSIGNED_SHORT ? 2 : 4;
    auto* offset = static_cast<const unsigned char*>(indices);
    for (auto i = GLsizei{0}; i + 3 < count; i += 4)
    {
      m_gl.glDrawElements(GL_TRIANGLE_FAN, 4, type, offset + i * indexSize);
    }
    return;
  }
#endif
  m_gl.glDrawElements(mode, count, type, indices);
}

void GlQt::multiDrawArrays(
  const GLenum mode, const GLint* first, const GLsizei* count, const GLsizei primcount)
{
#if !defined(Q_OS_ANDROID)
  m_gl.glMultiDrawArrays(mode, first, count, primcount);
#else
  for (auto i = GLsizei{0}; i < primcount; ++i)
  {
    drawArrays(mode, first[i], count[i]);
  }
#endif
}

const GLubyte* GlQt::getString(const GLenum name)
{
  return m_gl.glGetString(name);
}

GLenum GlQt::getError()
{
  return m_gl.glGetError();
}

} // namespace tb::ui
