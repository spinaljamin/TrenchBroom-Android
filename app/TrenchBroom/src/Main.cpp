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

#include <QAbstractButton>
#include <QApplication>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QFile>
#include <QWidget>
#include <QKeyEvent>
#include <QFileDialog>
#include <QEvent>
#include <QDialog>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPalette>
#include <QPointer>
#include <QProxyStyle>
#include <QSettings>
#include <QString>
#include <QStringList>
#include <QStyleHints>
#include <QSurfaceFormat>
#include <QTimer>
#include <QtGlobal>
#include <QtSystemDetection>
#if defined(Q_OS_ANDROID)
#include <QtCore/qcoreapplication_platform.h>
#include <QtCore/qjnienvironment.h>
#include <QtCore/qjniobject.h>
#endif

#include "PreferenceManager.h"
#include "Preferences.h"
#include "ui/Action.h"
#include "ui/ActionBuilder.h"
#include "ui/ActionExecutionContext.h"
#include "ui/AppController.h"
#include "ui/Contracts.h"
#include "ui/CrashReporter.h"
#include "ui/FileEventFilter.h"
#include "ui/MapWindowManager.h"
#include "ui/QPathUtils.h"
#include "ui/QPreferenceStore.h"
#include "ui/RecentDocuments.h"
#include "ui/SystemPaths.h"

using namespace tb;
using namespace tb::ui;

static_assert(
  QT_VERSION >= QT_VERSION_CHECK(6, 7, 0), "TrenchBroom Android test build requires Qt 6.7.0 or later");

extern void qt_set_sequence_auto_mnemonic(bool b);

namespace
{
#if defined(Q_OS_ANDROID)
class AndroidMouseReleaseEventFilter : public QObject
{
private:
  QPointer<QWidget> m_pressedWidget;
  QPointF m_localPosition;
  QPointF m_scenePosition;
  QPointF m_globalPosition;
  Qt::MouseButton m_button = Qt::NoButton;
  Qt::KeyboardModifiers m_modifiers;

public:
  using QObject::QObject;

  bool eventFilter(QObject* watched, QEvent* event) override
  {
    auto* widget = qobject_cast<QWidget*>(watched);
    if (!widget)
    {
      return false;
    }

    if (event->type() == QEvent::MouseButtonPress)
    {
      const auto* mouseEvent = static_cast<QMouseEvent*>(event);
      if (mouseEvent->button() != Qt::NoButton)
      {
        m_pressedWidget = widget;
        m_localPosition = mouseEvent->position();
        m_scenePosition = mouseEvent->scenePosition();
        m_globalPosition = mouseEvent->globalPosition();
        m_button = mouseEvent->button();
        m_modifiers = mouseEvent->modifiers();
      }
    }
    else if (event->type() == QEvent::MouseMove && m_pressedWidget == widget)
    {
      const auto* mouseEvent = static_cast<QMouseEvent*>(event);
      m_localPosition = mouseEvent->position();
      m_scenePosition = mouseEvent->scenePosition();
      m_globalPosition = mouseEvent->globalPosition();
      m_modifiers = mouseEvent->modifiers();
    }
    else if (event->type() == QEvent::MouseButtonRelease)
    {
      m_pressedWidget = nullptr;
      m_button = Qt::NoButton;
    }

    return false;
  }

  void synthesizeMouseRelease()
  {
    auto* target = m_pressedWidget.data();
    const auto button = m_button;
    m_pressedWidget = nullptr;
    m_button = Qt::NoButton;
    if (!target || button == Qt::NoButton)
    {
      return;
    }

    auto releaseEvent = QMouseEvent{
      QEvent::MouseButtonRelease,
      m_localPosition,
      m_scenePosition,
      m_globalPosition,
      button,
      Qt::NoButton,
      m_modifiers};
    QCoreApplication::sendEvent(target, &releaseEvent);
  }
};

AndroidMouseReleaseEventFilter* g_androidMouseReleaseFilter = nullptr;

class AndroidFileDialogEventFilter : public QObject
{
public:
  using QObject::QObject;

  bool eventFilter(QObject* watched, QEvent* event) override
  {
    auto* widget = qobject_cast<QWidget*>(watched);
    if (!widget)
    {
      return false;
    }

    auto* fileDialog = qobject_cast<QFileDialog*>(widget);
    for (auto* parent = widget->parentWidget(); !fileDialog && parent; parent = parent->parentWidget())
    {
      fileDialog = qobject_cast<QFileDialog*>(parent);
    }

    if (!fileDialog)
    {
      return false;
    }

    if (auto* button = qobject_cast<QAbstractButton*>(widget))
    {
      if (event->type() == QEvent::MouseButtonPress)
      {
        const auto pressed = button->isEnabled();
        button->setDown(false);
        if (pressed)
        {
          button->click();
        }
        event->accept();
        return true;
      }
      if (event->type() == QEvent::MouseButtonRelease)
      {
        button->setDown(false);
        event->accept();
        return true;
      }
    }

    if (event->type() == QEvent::KeyPress)
    {
      const auto* keyEvent = static_cast<QKeyEvent*>(event);
      if (keyEvent->key() == Qt::Key_Escape)
      {
        fileDialog->reject();
        event->accept();
        return true;
      }
    }

    return false;
  }
};

void installAndroidFileDialogEventFilter()
{
  qApp->installEventFilter(new AndroidFileDialogEventFilter{qApp});
  g_androidMouseReleaseFilter = new AndroidMouseReleaseEventFilter{qApp};
  qApp->installEventFilter(g_androidMouseReleaseFilter);
}
void requestAndroidStoragePermissions()
{
  if (!QNativeInterface::QAndroidApplication::isActivityContext())
  {
    return;
  }

  auto env = QJniEnvironment{};
  const auto stringClass = env->FindClass("java/lang/String");
  const auto permissions = env->NewObjectArray(2, stringClass, nullptr);
  const auto readPermission = QJniObject::fromString("android.permission.READ_EXTERNAL_STORAGE");
  const auto writePermission = QJniObject::fromString("android.permission.WRITE_EXTERNAL_STORAGE");
  env->SetObjectArrayElement(permissions, 0, readPermission.object<jstring>());
  env->SetObjectArrayElement(permissions, 1, writePermission.object<jstring>());

  auto activity = QJniObject{QNativeInterface::QAndroidApplication::context()};
  activity.callMethod<void>(
    "requestPermissions", "([Ljava/lang/String;I)V", permissions, jint(1001));
  env->DeleteLocalRef(permissions);
  env->DeleteLocalRef(stringClass);
}
#else
void requestAndroidStoragePermissions() {}
#endif
bool loadStyleSheets()
{
  const auto path = SystemPaths::findResourceFile("stylesheets/base.qss");
  if (auto file = QFile{pathAsQPath(path)}; file.exists())
  {
    // closed automatically by destructor
    contract_assert(file.open(QFile::ReadOnly | QFile::Text));
    qApp->setStyleSheet(QTextStream{&file}.readAll());

    return true;
  }
  return false;
}

QPalette darkPalette()
{
  const auto button = QColor{35, 35, 35};
  const auto text = QColor{207, 207, 207};
  const auto highlight = QColor{62, 112, 205};

  // Build an initial palette based on the button color
  auto palette = QPalette{button};

  // Window colors
  palette.setColor(QPalette::Active, QPalette::Window, QColor{50, 50, 50});
  palette.setColor(QPalette::Inactive, QPalette::Window, QColor{40, 40, 40});
  palette.setColor(QPalette::Disabled, QPalette::Window, QColor{50, 50, 50}.darker(200));

  // List box backgrounds, text entry backgrounds, menu backgrounds
  palette.setColor(QPalette::Base, button.darker(130));

  // Button text
  palette.setColor(QPalette::Active, QPalette::ButtonText, text);
  palette.setColor(QPalette::Inactive, QPalette::ButtonText, text);
  palette.setColor(QPalette::Disabled, QPalette::ButtonText, text.darker(200));

  // WindowText is supposed to be against QPalette::Window
  palette.setColor(QPalette::Active, QPalette::WindowText, text);
  palette.setColor(QPalette::Inactive, QPalette::WindowText, text);
  palette.setColor(QPalette::Disabled, QPalette::WindowText, text.darker(200));

  // Menu text, text edit text, table cell text
  palette.setColor(QPalette::Active, QPalette::Text, text.darker(115));
  palette.setColor(QPalette::Inactive, QPalette::Text, text.darker(115));

  // Disabled menu item text color
  palette.setColor(QPalette::Disabled, QPalette::Text, QColor{102, 102, 102});

  // Disabled menu item text shadow
  palette.setColor(QPalette::Disabled, QPalette::Light, button.darker(200));

  // Highlight (selected list box row, selected grid cell background, selected tab text
  palette.setColor(QPalette::Active, QPalette::Highlight, highlight);
  palette.setColor(QPalette::Inactive, QPalette::Highlight, highlight);
  palette.setColor(QPalette::Disabled, QPalette::Highlight, highlight);

  return palette;
}

void loadStyle(QApplication& app)
{
  // We can't use auto mnemonics in TrenchBroom. e.g. by default with Qt, Alt+D opens
  // the "Debug" menu, Alt+S activates the "Show default properties" checkbox in the
  // entity inspector. Flying with Alt held down and pressing WASD is a fundamental
  // behaviour in TB, so we can't have shortcuts randomly activating.
  //
  // Previously were calling `qt_set_sequence_auto_mnemonic(false);` in main(), but it
  // turns out we also need to suppress an Alt press followed by release from focusing
  // the menu bar (https://github.com/TrenchBroom/TrenchBroom/issues/3140), so the
  // following QProxyStyle disables that completely.

  class TrenchBroomProxyStyle : public QProxyStyle
  {
  public:
    explicit TrenchBroomProxyStyle(const QString& key)
      : QProxyStyle{key}
    {
    }

    explicit TrenchBroomProxyStyle(QStyle* style = nullptr)
      : QProxyStyle{style}
    {
    }

    int styleHint(
      StyleHint hint,
      const QStyleOption* option = nullptr,
      const QWidget* widget = nullptr,
      QStyleHintReturn* returnData = nullptr) const override
    {
#if defined(Q_OS_ANDROID)
      if (hint == QStyle::SH_Menu_SubMenuPopupDelay)
      {
        return 0;
      }
#endif
      return hint == QStyle::SH_MenuBar_AltKeyNavigation
               ? 0
               : QProxyStyle::styleHint(hint, option, widget, returnData);
    }
  };

  // Apply either the Fusion style + dark palette, or the system style
  if (pref(Preferences::Theme) == Preferences::DarkTheme)
  {
    app.setStyle(new TrenchBroomProxyStyle{"Fusion"});
    app.setPalette(darkPalette());
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    app.styleHints()->setColorScheme(Qt::ColorScheme::Dark);
#endif
  }
  else
  {
    // System
    app.setStyle(new TrenchBroomProxyStyle{});
  }
}
auto createAppController()
{
  return AppController::create() | kdl::if_error([](auto e) {
           const auto msg =
             fmt::format(R"(Game configurations could not be loaded: {})", e.msg);

           QMessageBox::critical(
             nullptr, "TrenchBroom", QString::fromStdString(msg), QMessageBox::Ok);
           QCoreApplication::exit(1);
         })
         | kdl::value();
}


[[maybe_unused]] void populateMainMenu(AppController& appController)
{
  auto* menuBar = new QMenuBar{};
#if defined(Q_OS_ANDROID)
  menuBar->setNativeMenuBar(false);
#endif
  auto actionMap = std::unordered_map<const Action*, QAction*>{};

  auto menuBuilderResult = populateMenuBar(
    appController.actionManager(), *menuBar, actionMap, [&](const Action& action) {
      auto context = ActionExecutionContext{appController, nullptr, nullptr};
      action.execute(context);
    });

  appController.recentDocuments().addMenu(*menuBuilderResult.recentDocumentsMenu);

  auto context = ActionExecutionContext{appController, nullptr, nullptr};
  for (auto [tbAction, qtAction] : actionMap)
  {
    qtAction->setEnabled(tbAction->enabled(context));
    if (qtAction->isCheckable())
    {
      qtAction->setChecked(tbAction->checked(context));
    }
  }
}

[[maybe_unused]] void installFileEventFilter(AppController& appController)
{
  qApp->installEventFilter(new FileEventFilter{appController, qApp});
}

bool openFiles(AppController& appController, const QStringList& fileNames)
{
  const auto filesToOpen = AppController::useSDI && !fileNames.empty()
                             ? QStringList{fileNames.front()}
                             : fileNames;

  auto anyDocumentOpened = false;
  for (const auto& fileName : filesToOpen)
  {
    const auto path = ui::pathFromQString(fileName);
    if (!path.empty() && appController.openDocument(path))
    {
      anyDocumentOpened = true;
    }
  }

  return anyDocumentOpened;
}

bool parseCommandLineAndOpenFiles(AppController& appController)
{
  auto parser = QCommandLineParser{};
  parser.addOption(QCommandLineOption("portable"));
  parser.addOption(QCommandLineOption("enableDraftReleaseUpdates"));
  parser.process(*qApp);

  if (parser.isSet("enableDraftReleaseUpdates"))
  {
    auto& prefs = PreferenceManager::instance();
    prefs.set(Preferences::EnableDraftReleaseUpdates, true);
    prefs.set(Preferences::IncludeDraftReleaseUpdates, true);
    prefs.saveChanges();
  }

  return openFiles(appController, parser.positionalArguments());
}


} // namespace

#if defined(Q_OS_ANDROID)
extern "C" JNIEXPORT void JNICALL
Java_org_trenchbroom_TrenchBroomActivity_nativeMouseButtonReleased(JNIEnv*, jclass)
{
  if (g_androidMouseReleaseFilter)
  {
    QMetaObject::invokeMethod(
      g_androidMouseReleaseFilter,
      []() {
        if (g_androidMouseReleaseFilter)
        {
          g_androidMouseReleaseFilter->synthesizeMouseRelease();
        }
      },
      Qt::QueuedConnection);
  }
}
#endif

int main(int argc, char* argv[])
{
#if defined(Q_OS_ANDROID)
  Q_INIT_RESOURCE(android_bundled_resources);
  Q_INIT_RESOURCE(resources);
#endif
  // Set OpenGL defaults
  // Needs to be done here before QApplication is created
  // (see: https://doc.qt.io/qt-5/qsurfaceformat.html#setDefaultFormat)
  QSurfaceFormat format;
#if defined(Q_OS_ANDROID)
  format.setRenderableType(QSurfaceFormat::OpenGLES);
  format.setVersion(3, 0);
  format.setProfile(QSurfaceFormat::NoProfile);
  format.setSamples(0);
#else
  format.setRenderableType(QSurfaceFormat::OpenGL);
  format.setVersion(2, 1);
  format.setProfile(QSurfaceFormat::CompatibilityProfile);
  format.setSamples(4);
#endif
  format.setDepthBufferSize(24);
  QSurfaceFormat::setDefaultFormat(format);

  // Makes all QOpenGLWidget in the application share a single context
  // (default behaviour would be for QOpenGLWidget's in a single top-level window to share
  // a context.) see: http://doc.qt.io/qt-5/qopenglwidget.html#context-sharing
  QApplication::setAttribute(Qt::AA_ShareOpenGLContexts);

  // Set up Hi DPI scaling
  // Enables non-integer scaling (e.g. 150% scaling on Windows)
  QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
    Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

  // When this flag is enabled, font and palette changes propagate as though the user
  // had manually called the corresponding QWidget methods.
  QGuiApplication::setAttribute(Qt::AA_UseStyleSheetPropagationInWidgetStyles);

  // Don't show icons in menus, they are scaled down and don't look very good.
  QGuiApplication::setAttribute(Qt::AA_DontShowIconsInMenus);

  // Store settings in INI format
  QSettings::setDefaultFormat(QSettings::IniFormat);

  // Workaround bug in Qt's Ctrl+Click = RMB emulation (a macOS feature.)
  // In Qt 5.13.0 / macOS 10.14.6, Ctrl+trackpad click+Drag produces no mouse events at
  // all, but it should produce RMB down/move events. This environment variable disables
  // Qt's emulation so we can implement it ourselves in InputEventRecorder::recordEvent
  qputenv("QT_MAC_DONT_OVERRIDE_CTRL_LMB", "1");

  // Disable Qt OpenGL buglist; since we require desktop OpenGL 2.1 there's no point in
  // having Qt disable it (also we've had reports of some Intel drivers being blocked that
  // actually work with TB.)
  qputenv("QT_OPENGL_BUGLIST", ":/opengl_buglist.json");

  // parse portable arg out manually at first to ensure it's set before any settings load
  if (argc > 1)
  {
    for (int i = 1; i < argc; i++)
    {
      if (strcmp(argv[i], "--portable") == 0)
      {
        SystemPaths::setPortable();
        QSettings::setPath(
          QSettings::IniFormat, QSettings::UserScope, QString("./config"));
      }
    }
  }

  // Needs to be set before creating the preference manager
  QApplication::setApplicationName("TrenchBroom");
  // Needs to be "" otherwise Qt adds this to the paths returned by QStandardPaths
  // which would cause preferences to move from where they were with wx
  QApplication::setOrganizationName("");
  QApplication::setOrganizationDomain("io.github.trenchbroom");

  // QApplication must be created before QPreferenceStore because QPreferenceStore uses
  // QFileSystemWatcher, which requires a QApplication instance
  auto app = QApplication{argc, argv};
  installAndroidFileDialogEventFilter();

  // PreferenceManager is destroyed by TrenchBroomApp::~TrenchBroomApp()
  PreferenceManager::createInstance(
    std::make_unique<QPreferenceStore>(pathAsQString(SystemPaths::preferenceFilePath())));

  // Style sheets must be loaded before creating the app controller, or they won't apply
  // to the welcome window, which the app controller creates
  loadStyleSheets();
  loadStyle(app);

  auto appController = createAppController();
  auto crashReporter = CrashReporter{*appController};
  setContractViolationHandler(crashReporter);

#ifdef __APPLE__
  app.setQuitOnLastWindowClosed(false);
  populateMainMenu(*appController);
  installFileEventFilter(*appController);
#endif

#if defined(Q_OS_ANDROID)
  QTimer::singleShot(1500, []() { requestAndroidStoragePermissions(); });
#else
  appController->askForAutoUpdates();
  appController->triggerAutoUpdateCheck();
#endif

  if (!parseCommandLineAndOpenFiles(*appController))
  {
#if defined(Q_OS_ANDROID)
    QTimer::singleShot(0, [&]() {
      if (!appController->newDocument())
      {
        appController->showWelcomeWindow();
      }
    });
#else
    appController->showWelcomeWindow();
#endif
  }

  return app.exec();
}
