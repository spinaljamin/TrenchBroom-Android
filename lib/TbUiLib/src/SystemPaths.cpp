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

#include "ui/SystemPaths.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <QString>
#include <QtSystemDetection>

#include "fs/DiskIO.h"
#include "fs/PathInfo.h"
#include "ui/QPathUtils.h"

#include "kd/optional_utils.h"

#include <algorithm>
#include <cstdlib>
#include <vector>

namespace tb::ui::SystemPaths
{
namespace
{

std::filesystem::path appImageDirectory()
{
  return appDirectory() / ".." / "share" / "TrenchBroom";
}

#if defined(Q_OS_ANDROID)
std::filesystem::path bundledResourceDirectory()
{
  return userDataDirectory() / "bundled-resources";
}

void extractBundledResources()
{
  static auto extracted = false;
  if (extracted)
  {
    return;
  }
  extracted = true;

  const auto sourceRoot = QString{":/tb_android_resources"};
  const auto destinationRoot = pathAsQString(bundledResourceDirectory());
  QDir{}.mkpath(destinationRoot);

  auto iterator = QDirIterator{sourceRoot, QDirIterator::Subdirectories};
  const auto sourceRootDir = QDir{sourceRoot};
  while (iterator.hasNext())
  {
    const auto sourcePath = iterator.next();
    const auto sourceInfo = QFileInfo{sourcePath};
    if (!sourceInfo.isFile())
    {
      continue;
    }

    const auto relativePath = sourceRootDir.relativeFilePath(sourcePath);
    const auto destinationPath = QDir{destinationRoot}.filePath(relativePath);
    QDir{}.mkpath(QFileInfo{destinationPath}.absolutePath());

    QFile::remove(destinationPath);
    QFile::copy(sourcePath, destinationPath);
    QFile::setPermissions(
      destinationPath,
      QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ReadUser
        | QFileDevice::WriteUser | QFileDevice::ReadGroup | QFileDevice::ReadOther);
  }
}
#endif

} // namespace

std::filesystem::path appFile()
{
  return pathFromQString(QCoreApplication::applicationFilePath());
}

std::filesystem::path appDirectory()
{
  return pathFromQString(QCoreApplication::applicationDirPath());
}

std::optional<std::filesystem::path> appImageFile()
{
#if defined(Q_OS_LINUX) || defined(Q_OS_FREEBSD)
  if (const auto* appImagePath = std::getenv("APPIMAGE"))
  {
    if (const auto appImagePathStr = std::string{appImagePath}; !appImagePathStr.empty())
    {
      return std::filesystem::path{appImagePathStr};
    }
  }
#endif

  return std::nullopt;
}

std::filesystem::path userDataDirectory()
{
#if defined(Q_OS_ANDROID)
  return pathFromQString(
    QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
#endif

  if (isPortable())
  {
    const auto parentPath =
      appImageFile()
      | kdl::optional_transform([](const auto& path) { return path.parent_path(); })
      | kdl::optional_value_or(appDirectory());
    return parentPath / "config";
  }

#if defined(Q_OS_LINUX) || defined(Q_OS_FREEBSD)
  // Compatibility with wxWidgets
  return pathFromQString(QDir::homePath()) / ".TrenchBroom";
#else
  return pathFromQString(
    QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
#endif
}

std::filesystem::path userGamesDirectory()
{
  return userDataDirectory() / "games";
}

std::filesystem::path tempDirectory()
{
  return pathFromQString(QStandardPaths::writableLocation(QStandardPaths::TempLocation));
}

std::filesystem::path logFilePath()
{
  return userDataDirectory() / "TrenchBroom.log";
}

std::filesystem::path preferenceFilePath()
{
  return userDataDirectory() / "Preferences.json";
}

std::filesystem::path findResourceFile(const std::filesystem::path& file)
{
#if defined(Q_OS_ANDROID)
  extractBundledResources();
  const auto inBundledResources = bundledResourceDirectory() / file;
  if (fs::Disk::pathInfo(inBundledResources) == fs::PathInfo::File)
  {
    return inBundledResources;
  }
#endif
  // Special case for running debug builds on Linux, we want to search
  // next to the executable for resources
  const auto relativeToExecutable = appDirectory() / file;
  if (fs::Disk::pathInfo(relativeToExecutable) == fs::PathInfo::File)
  {
    return relativeToExecutable;
  }

  // Compatibility with wxWidgets
  const auto inUserDataDir = userDataDirectory() / file;
  if (fs::Disk::pathInfo(inUserDataDir) == fs::PathInfo::File)
  {
    return inUserDataDir;
  }

  // Compatibility with AppImage runtime
  const auto inAppImageDir = appImageDirectory() / file;
  if (fs::Disk::pathInfo(inAppImageDir) == fs::PathInfo::File)
  {
    return inAppImageDir;
  }

  return pathFromQString(QStandardPaths::locate(
    QStandardPaths::AppDataLocation,
    pathAsQPath(file),
    QStandardPaths::LocateOption::LocateFile));
}

std::vector<std::filesystem::path> findResourceDirectories(
  const std::filesystem::path& directory)
{
#if defined(Q_OS_ANDROID)
  extractBundledResources();
#endif

  auto result = std::vector<std::filesystem::path>{
#if defined(Q_OS_ANDROID)
    bundledResourceDirectory() / directory,
#endif
    // Special case for running debug builds on Linux
    appDirectory() / directory,
    // Compatibility with wxWidgets
    userDataDirectory() / directory,
    // Compatibility with AppImage
    appImageDirectory() / directory,
  };

  const auto dirs = QStandardPaths::locateAll(
    QStandardPaths::AppDataLocation,
    pathAsQPath(directory),
    QStandardPaths::LocateOption::LocateDirectory);

  for (const auto& dir : dirs)
  {
    const auto path = pathFromQString(dir);
    if (std::ranges::find(result, path) == result.end())
    {
      result.push_back(path);
    }
  }
  return result;
}

bool portableState = false;

bool isPortable()
{
  return portableState;
}

void setPortable()
{
  setPortable(true);
}

void setPortable(bool newState)
{
  portableState = newState;
}

} // namespace tb::ui::SystemPaths
