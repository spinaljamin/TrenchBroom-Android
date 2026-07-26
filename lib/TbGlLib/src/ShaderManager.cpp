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

#include "gl/ShaderManager.h"

#include "gl/ShaderConfig.h"

#include "kd/contracts.h"
#include "kd/result.h"
#include "kd/result_fold.h"

#include <filesystem>
#include <ranges>
#include <string>
#include <variant>

namespace tb::gl
{

ShaderManager::ShaderManager(FindShaderFunc findShaderFunc)
  : m_findShaderFunc{std::move(findShaderFunc)}
{
}

Result<void> ShaderManager::loadProgram(Gl& gl, const ShaderConfig& config)
{
  return createProgram(gl, config) | kdl::and_then([&](auto program) -> Result<void> {
           if (!m_programs.emplace(config.name, std::move(program)).second)
           {
             return Error{"Shader program '" + config.name + "' already loaded"};
           }
           return kdl::void_success;
         });
}

ShaderProgram& ShaderManager::program(const ShaderConfig& config)
{
  auto it = m_programs.find(config.name);
  contract_assert(it != std::end(m_programs));

  return it->second;
}

ShaderProgram* ShaderManager::currentProgram()
{
  return m_currentProgram;
}

void ShaderManager::setCurrentProgram(ShaderProgram* program)
{
  m_currentProgram = program;
}

Result<ShaderProgram> ShaderManager::createProgram(Gl& gl, const ShaderConfig& config)
{
#if defined(ANDROID)
  return createShaderProgram(gl, config.name) | kdl::and_then([&](auto program) {
           auto vertexPaths = std::vector<std::filesystem::path>{};
           for (const auto& path : config.vertexShaders)
           {
             vertexPaths.push_back(m_findShaderFunc(std::filesystem::path{"shader"} / path));
           }
           auto vertexShader = tb::gl::loadShader(
             gl, config.name + " vertex stage", vertexPaths, GL_VERTEX_SHADER);
           if (vertexShader.is_error())
           {
             return Result<ShaderProgram>{std::get<Error>(std::move(vertexShader).error())};
           }
           auto vertexShaderValue = std::move(vertexShader).value();
           program.attach(gl, vertexShaderValue);

           auto fragmentPaths = std::vector<std::filesystem::path>{};
           for (const auto& path : config.fragmentShaders)
           {
             fragmentPaths.push_back(m_findShaderFunc(std::filesystem::path{"shader"} / path));
           }
           auto fragmentShader = tb::gl::loadShader(
             gl, config.name + " fragment stage", fragmentPaths, GL_FRAGMENT_SHADER);
           if (fragmentShader.is_error())
           {
             return Result<ShaderProgram>{std::get<Error>(std::move(fragmentShader).error())};
           }
           auto fragmentShaderValue = std::move(fragmentShader).value();
           program.attach(gl, fragmentShaderValue);

           return program.link(gl)
                  | kdl::transform([&]() { return std::move(program); });
         });
#else
  return createShaderProgram(gl, config.name) | kdl::and_then([&](auto program) {
           return config.vertexShaders | std::views::transform([&](const auto& path) {
                    return loadShader(gl, path, GL_VERTEX_SHADER)
                           | kdl::transform(
                             [&](auto shader) { program.attach(gl, shader.get()); });
                  })
                  | kdl::fold | kdl::transform([&]() { return std::move(program); });
         })
         | kdl::and_then([&](auto program) {
             return config.fragmentShaders | std::views::transform([&](const auto& path) {
                      return loadShader(gl, path, GL_FRAGMENT_SHADER)
                             | kdl::transform(
                               [&](auto shader) { program.attach(gl, shader.get()); });
                    })
                    | kdl::fold | kdl::transform([&]() { return std::move(program); });
           })
         | kdl::and_then([&](auto program) {
             return program.link(gl)
                    | kdl::transform([&]() { return std::move(program); });
           });
#endif
}

Result<std::reference_wrapper<Shader>> ShaderManager::loadShader(
  Gl& gl, const std::string& name, const GLenum type)
{
  auto it = m_shaders.find(name);
  if (it != std::end(m_shaders))
  {
    return std::ref(it->second);
  }

  const auto shaderPath = m_findShaderFunc(std::filesystem::path{"shader"} / name);
  return gl::loadShader(gl, shaderPath, type) | kdl::transform([&](auto shader) {
           const auto [insertIt, inserted] = m_shaders.emplace(name, std::move(shader));
           contract_assert(inserted);

           return std::ref(insertIt->second);
         });
}

} // namespace tb::gl
