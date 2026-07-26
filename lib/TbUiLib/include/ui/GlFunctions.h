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

#pragma once

#include <string_view>

#include <QtGlobal>

#if defined(Q_OS_ANDROID)
#include <QOpenGLFunctions>
#else
class QOpenGLFunctions_2_1;
#endif

class QOpenGLContext;

namespace tb::ui
{

#if defined(Q_OS_ANDROID)
using OpenGLFunctions = QOpenGLFunctions;
#else
using OpenGLFunctions = QOpenGLFunctions_2_1;
#endif

OpenGLFunctions& getGlFunctions(std::string_view callSite, QOpenGLContext* context);

} // namespace tb::ui