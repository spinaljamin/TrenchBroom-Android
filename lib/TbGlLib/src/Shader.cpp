/*
 Copyright (C) 2010 Kristian Duske

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

#include "gl/Shader.h"

#include "fs/DiskIO.h"
#include "gl/GlInterface.h"

#include "kd/contracts.h"
#include "kd/ranges/to.h"
#include "kd/result.h"

#include <ranges>
#include <string>
#include <vector>
#include <variant>

namespace tb::gl
{

Shader::Shader(std::string name, const GLenum type, const GLuint shaderId)
  : m_name{std::move(name)}
  , m_type{type}
  , m_shaderId{shaderId}
{
  contract_pre(m_type == GL_VERTEX_SHADER || m_type == GL_FRAGMENT_SHADER);
  contract_pre(m_shaderId != 0);
}

Shader::Shader(Shader&& other) noexcept
  : m_name{std::move(other.m_name)}
  , m_type{other.m_type}
  , m_shaderId{std::exchange(other.m_shaderId, 0)}
{
}

Shader& Shader::operator=(Shader&& other) noexcept
{
  m_name = std::move(other.m_name);
  m_type = other.m_type;
  m_shaderId = std::exchange(other.m_shaderId, 0);
  return *this;
}

void Shader::attach(Gl& gl, const GLuint programId) const
{
  contract_pre(m_shaderId != 0);

  gl.attachShader(programId, m_shaderId);
}

void Shader::destroy(Gl& gl)
{
  if (m_shaderId != 0)
  {
    gl.deleteShader(m_shaderId);
    m_shaderId = 0;
  }
}

namespace
{

Result<std::vector<std::string>> loadSource(const std::filesystem::path& path)
{
  return fs::Disk::withInputStream(path, [](auto& stream) {
    std::string line;
    std::vector<std::string> lines;

    while (!stream.eof())
    {
      std::getline(stream, line);
      lines.push_back(line + '\n');
    }

    return lines;
  });
}

#if defined(ANDROID)
void replaceAll(std::string& str, const std::string& from, const std::string& to)
{
  auto pos = size_t{0};
  while ((pos = str.find(from, pos)) != std::string::npos)
  {
    str.replace(pos, from.length(), to);
    pos += to.length();
  }
}

size_t findMatchingBrace(const std::string& str, const size_t openBrace)
{
  auto depth = 0;
  for (auto i = openBrace; i < str.size(); ++i)
  {
    if (str[i] == '{')
    {
      ++depth;
    }
    else if (str[i] == '}')
    {
      --depth;
      if (depth == 0)
      {
        return i;
      }
    }
  }
  return std::string::npos;
}

std::vector<std::string> translateShaderSourceForAndroid(
  const std::vector<std::string>& source, const GLenum type)
{
  auto body = std::string{};
  for (const auto& line : source)
  {
    if (line.rfind("#version", 0) != 0)
    {
      body += line;
    }
  }

  replaceAll(body, "varying", type == GL_VERTEX_SHADER ? "out" : "in");
  if (type == GL_VERTEX_SHADER)
  {
    replaceAll(body, "attribute", "in");
  }
  replaceAll(body, "texture2D", "texture");
  if (type == GL_FRAGMENT_SHADER)
  {
    replaceAll(body, "gl_FragColor", "tb_FragColorCompat");
    replaceAll(body, "2 * tb_FragColorCompat", "2.0 * tb_FragColorCompat");

    const auto mainPos = body.find("void main");
    if (mainPos != std::string::npos)
    {
      const auto mainOpenBrace = body.find('{', mainPos);
      if (mainOpenBrace != std::string::npos)
      {
        body.insert(mainOpenBrace + 1, "\n  tb_FragColorCompat = vec4(0.0);\n");

        const auto mainCloseBrace = findMatchingBrace(body, mainOpenBrace);
        if (mainCloseBrace != std::string::npos && mainCloseBrace > mainOpenBrace)
        {
          body.insert(mainCloseBrace, "\n  tb_FragColor = tb_FragColorCompat;\n");
        }
      }
    }
  }
  else
  {
    replaceAll(body, "gl_FragColor", "tb_FragColor");
  }
  replaceAll(body, "gl_FrontColor", "tb_FrontColor");
  replaceAll(body, "gl_Color", type == GL_VERTEX_SHADER ? "tb_Color" : "tb_FrontColor");
  replaceAll(body, "gl_Vertex", "tb_Vertex");
  replaceAll(body, "gl_Normal", "tb_Normal");
  replaceAll(body, "gl_MultiTexCoord0", "tb_MultiTexCoord0");
  replaceAll(body, "gl_ProjectionMatrix", "tb_ProjectionMatrix");
  replaceAll(body, "gl_ModelViewMatrix", "tb_ModelViewMatrix");
  replaceAll(body, "gl_NormalMatrix", "tb_NormalMatrix");
  replaceAll(body, "gl_TexCoord", "tb_TexCoord");

  auto translated = std::vector<std::string>{};
  if (type == GL_VERTEX_SHADER)
  {
    translated.push_back(
      "#version 300 es\n"
      "precision highp float;\n"
      "precision highp int;\n"
      "layout(location = 0) in vec4 tb_Vertex;\n"
      "layout(location = 1) in vec3 tb_Normal;\n"
      "layout(location = 2) in vec4 tb_Color;\n"
      "layout(location = 3) in vec4 tb_MultiTexCoord0;\n"
      "uniform mat4 tb_ProjectionMatrix;\n"
      "uniform mat4 tb_ModelViewMatrix;\n"
      "uniform mat3 tb_NormalMatrix;\n"
      "out vec4 tb_TexCoord[1];\n"
      "out vec4 tb_FrontColor;\n");
  }
  else
  {
    translated.push_back(
      "#version 300 es\n"
      "precision highp float;\n"
      "precision highp int;\n"
      "in vec4 tb_TexCoord[1];\n"
      "in vec4 tb_FrontColor;\n"
      "out vec4 tb_FragColor;\n"
      "vec4 tb_FragColorCompat;\n");
  }
  translated.push_back(std::move(body));
  return translated;
}
#endif

std::string getInfoLog(Gl& gl, const GLuint shaderId)
{
  auto infoLogLength = GLint{};
  gl.getShaderiv(shaderId, GL_INFO_LOG_LENGTH, &infoLogLength);
  if (infoLogLength > 0)
  {
    auto infoLog = std::string{};
    infoLog.resize(size_t(infoLogLength));

    gl.getShaderInfoLog(shaderId, infoLogLength, &infoLogLength, infoLog.data());
    return infoLog;
  }

  return "Unknown error";
}

} // namespace

Result<Shader> loadShader(
  Gl& gl, std::string name, const std::vector<std::filesystem::path>& paths, const GLenum type)
{
  const auto shaderId = gl.createShader(type);

  if (shaderId == 0)
  {
    return Error{"Could not create shader " + name};
  }

  auto source = std::vector<std::string>{};
  for (const auto& path : paths)
  {
    auto pathSource = loadSource(path);
    if (pathSource.is_error())
    {
      return std::get<Error>(std::move(pathSource).error());
    }
    auto lines = std::move(pathSource).value();
    source.insert(std::end(source), std::make_move_iterator(std::begin(lines)), std::make_move_iterator(std::end(lines)));
  }

#if defined(ANDROID)
  const auto shaderSource = translateShaderSourceForAndroid(source, type);
#else
  const auto& shaderSource = source;
#endif
  const auto linePtrs =
    shaderSource | std::views::transform([](const auto& line) { return line.c_str(); })
    | kdl::ranges::to<std::vector>();

  gl.shaderSource(shaderId, GLsizei(linePtrs.size()), linePtrs.data(), nullptr);
  gl.compileShader(shaderId);

  auto compileStatus = GLint{};
  gl.getShaderiv(shaderId, GL_COMPILE_STATUS, &compileStatus);

  if (compileStatus == 0)
  {
    return Error{"Could not compile shader '" + name + "': " + getInfoLog(gl, shaderId)};
  }

  return Shader{std::move(name), type, shaderId};
}

Result<Shader> loadShader(Gl& gl, const std::filesystem::path& path, const GLenum type)
{
  return loadShader(gl, path.filename().string(), std::vector<std::filesystem::path>{path}, type);
}

} // namespace tb::gl
