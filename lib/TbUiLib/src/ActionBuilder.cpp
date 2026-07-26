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

#include "ui/ActionBuilder.h"

#include <QApplication>
#include <QDebug>
#include <QEvent>
#include <QMenu>
#include <QMenuBar>
#include <QMouseEvent>
#include <QPointer>
#include <QToolBar>
#include <QToolButton>
#include <QTimer>
#include <QtSystemDetection>

#include "PreferenceManager.h"
#include "ui/Action.h"
#include "ui/ActionManager.h"
#include "ui/ActionMenu.h"
#include "ui/ImageUtils.h"

#include "kd/contracts.h"
#include "kd/ranges/to.h"

namespace tb::ui
{

void updateActionKeySequence(QAction& qAction, const Action& tAction)
{
  const auto& keySequences = pref(tAction.preference());

  auto tooltip = tAction.label();
  for (const auto& keySequence : keySequences)
  {
    if (!keySequence.isEmpty())
    {
      tooltip.append(
        QObject::tr(" (%1)").arg(keySequence.toString(QKeySequence::NativeText)));
    }
  }

  qAction.setToolTip(tooltip);
  qAction.setShortcuts(keySequences | kdl::ranges::to<QList>());
}

namespace
{

#if defined(Q_OS_ANDROID)
class AndroidToolButtonPressFilter : public QObject
{
private:
  QAction& m_action;
  bool m_pressed = false;

public:
  explicit AndroidToolButtonPressFilter(QAction& action, QObject* parent)
    : QObject{parent}
    , m_action{action}
  {
  }

  bool eventFilter(QObject* watched, QEvent* event) override
  {
    auto* button = qobject_cast<QToolButton*>(watched);
    if (event->type() == QEvent::MouseButtonPress)
    {
      m_pressed = m_action.isEnabled();
      if (button)
      {
        button->setDown(false);
      }
      if (m_pressed)
      {
        m_action.trigger();
      }
      event->accept();
      return true;
    }
    if (event->type() == QEvent::MouseButtonRelease && m_pressed)
    {
      m_pressed = false;
      if (button)
      {
        button->setDown(false);
      }
      event->accept();
      return true;
    }
    return false;
  }
};

class AndroidMenuPressFilter : public QObject
{
private:
  QMenuBar& m_menuBar;
  QPointer<QWidget> m_previousFocusWidget;
  QMenu* m_pendingMenu = nullptr;
  QAction* m_lastMoveAction = nullptr;

  static QString actionText(QAction* action)
  {
    return action ? action->text() : QString{"<none>"};
  }

  static void logMenuEvent(
    const char* type, QMenu& menu, const QMouseEvent& mouseEvent, QAction* action)
  {
    qInfo().noquote()
      << "TB_ANDROID_MENU" << type << "menu" << menu.title() << "pos" << menu.pos()
      << "size" << menu.size() << "event" << mouseEvent.position() << "global"
      << mouseEvent.globalPosition() << "dpr" << menu.devicePixelRatioF() << "action"
      << actionText(action) << "actionRect" << (action ? menu.actionGeometry(action) : QRect{});
  }
  static void closeAndroidMenu(QMenu& menu)
  {
    menu.setActiveAction(nullptr);
    if (auto* grabber = QWidget::mouseGrabber();
        grabber && (grabber == &menu || menu.isAncestorOf(grabber)))
    {
      grabber->releaseMouse();
    }
    if (auto* grabber = QWidget::keyboardGrabber();
        grabber && (grabber == &menu || menu.isAncestorOf(grabber)))
    {
      grabber->releaseKeyboard();
    }
    menu.close();
  }

  void restorePreviousFocus()
  {
    const auto previousFocusWidget = m_previousFocusWidget;
    m_previousFocusWidget = nullptr;
    QTimer::singleShot(0, this, [previousFocusWidget]() {
      if (previousFocusWidget)
      {
        previousFocusWidget->setFocus(Qt::OtherFocusReason);
        previousFocusWidget->activateWindow();
      }
    });
  }

  void closeOtherMenus(const QMenu* except = nullptr)
  {
    for (auto* action : m_menuBar.actions())
    {
      auto* menu = action->menu();
      if (menu && menu != except && menu->isVisible())
      {
        closeAndroidMenu(*menu);
      }
    }
  }

  void openTopLevelMenu(QAction& menuBarAction)
  {
    auto* nextMenu = menuBarAction.menu();
    if (!nextMenu)
    {
      return;
    }

    if (!m_previousFocusWidget)
    {
      auto* focusWidget = QApplication::focusWidget();
      if (focusWidget && focusWidget != &m_menuBar && !qobject_cast<QMenu*>(focusWidget))
      {
        m_previousFocusWidget = focusWidget;
      }
    }

    m_pendingMenu = nextMenu;
    closeOtherMenus();
    auto* menuBarActionPtr = &menuBarAction;
    QTimer::singleShot(0, this, [this, menuBarActionPtr, nextMenu]() {
      if (m_pendingMenu != nextMenu)
      {
        return;
      }

      closeOtherMenus(nextMenu);
      const auto rect = m_menuBar.actionGeometry(menuBarActionPtr);
      nextMenu->popup(m_menuBar.mapToGlobal(QPoint{rect.left(), rect.bottom()}));
      m_pendingMenu = nullptr;
    });
  }

  bool switchTopLevelMenu(QMenu& currentMenu, const QPoint& globalPos)
  {
    const auto menuBarPos = m_menuBar.mapFromGlobal(globalPos);
    if (!m_menuBar.rect().contains(menuBarPos))
    {
      return false;
    }

    auto* menuBarAction = m_menuBar.actionAt(menuBarPos);
    auto* nextMenu = menuBarAction ? menuBarAction->menu() : nullptr;
    if (!nextMenu || nextMenu == &currentMenu)
    {
      return true;
    }

    qInfo().noquote()
      << "TB_ANDROID_MENU switch" << currentMenu.title() << "->" << nextMenu->title()
      << "barPos" << menuBarPos << "global" << globalPos;

    if (m_pendingMenu != nextMenu)
    {
      openTopLevelMenu(*menuBarAction);
    }
    return true;
  }

public:
  explicit AndroidMenuPressFilter(QMenuBar& menuBar, QObject* parent)
    : QObject{parent}
    , m_menuBar{menuBar}
  {
  }

  bool eventFilter(QObject* watched, QEvent* event) override
  {
    if (watched == &m_menuBar)
    {
      if (event->type() == QEvent::MouseButtonPress)
      {
        const auto* mouseEvent = static_cast<QMouseEvent*>(event);
        const auto menuBarPos =
          m_menuBar.mapFromGlobal(mouseEvent->globalPosition().toPoint());
        auto* action = m_menuBar.actionAt(menuBarPos);
        auto* menu = action ? action->menu() : nullptr;
        if (menu)
        {
          if (menu->isVisible())
          {
            m_pendingMenu = nullptr;
            closeAndroidMenu(*menu);
            restorePreviousFocus();
          }
          else
          {
            openTopLevelMenu(*action);
          }
          event->accept();
          return true;
        }
      }
      if (event->type() == QEvent::MouseButtonRelease)
      {
        event->accept();
        return true;
      }
      return false;
    }

    auto* menu = qobject_cast<QMenu*>(watched);
    if (!menu)
    {
      return false;
    }

    if (event->type() == QEvent::MouseMove)
    {
      const auto* mouseEvent = static_cast<QMouseEvent*>(event);
      auto menuPos = mouseEvent->position().toPoint();
      menuPos.ry() += menu->pos().y();
      auto* action = menu->actionAt(menuPos);
      if (action != m_lastMoveAction)
      {
        m_lastMoveAction = action;
        logMenuEvent("move", *menu, *mouseEvent, action);
      }
      menu->setActiveAction(
        action && action->isEnabled() && !action->isSeparator() ? action : nullptr);
      menu->update();
      event->accept();
      return true;
    }

    if (event->type() == QEvent::Leave)
    {
      m_lastMoveAction = nullptr;
      menu->setActiveAction(nullptr);
      menu->update();
      return false;
    }

    if (event->type() == QEvent::MouseButtonPress)
    {
      const auto* mouseEvent = static_cast<QMouseEvent*>(event);
      const auto menuPos = mouseEvent->position().toPoint();
      auto* action = menu->actionAt(menuPos);
      logMenuEvent("press", *menu, *mouseEvent, action);
      if (!menu->rect().contains(menuPos))
      {
        if (switchTopLevelMenu(*menu, mouseEvent->globalPosition().toPoint()))
        {
          event->accept();
          return true;
        }
        m_pendingMenu = nullptr;
        closeAndroidMenu(*menu);
        restorePreviousFocus();
        event->accept();
        return true;
      }
      if (action && action->isEnabled() && !action->isSeparator() && !action->menu())
      {
        menu->setActiveAction(action);
        closeAndroidMenu(*menu);
        closeOtherMenus();
        restorePreviousFocus();
        QTimer::singleShot(200, action, [action]() { action->trigger(); });
      }
      event->accept();
      return true;
    }

    if (event->type() == QEvent::MouseButtonRelease)
    {
      event->accept();
      return true;
    }

    if (
      event->type() == QEvent::Hide || event->type() == QEvent::Close
      || event->type() == QEvent::FocusOut || event->type() == QEvent::WindowDeactivate)
    {
      menu->releaseMouse();
      menu->releaseKeyboard();
      if (
        (event->type() == QEvent::Hide || event->type() == QEvent::Close)
        && !m_pendingMenu)
      {
        restorePreviousFocus();
      }
      m_lastMoveAction = nullptr;
      return false;
    }

    return false;
  }
};
void installAndroidMenuPressFilter(QMenu& menu, AndroidMenuPressFilter& filter)
{
  menu.setMouseTracking(true);
  menu.setStyleSheet("QMenu::item:disabled { color: rgb(104, 104, 104); }");
  menu.installEventFilter(&filter);
}
#endif
QAction& findOrCreateQtAction(
  std::unordered_map<const Action*, QAction*>& actionMap,
  const Action& tbAction,
  const TriggerFn& triggerFn)
{
  if (const auto it = actionMap.find(&tbAction); it != actionMap.end())
  {
    return *it->second;
  }

  auto& qtAction =
    *actionMap.emplace(&tbAction, new QAction{tbAction.label()}).first->second;

  qtAction.setCheckable(tbAction.checkable());
  if (const auto& iconPath = tbAction.iconPath())
  {
    qtAction.setIcon(loadSVGIcon(*iconPath));
  }
  if (const auto& statusTip = tbAction.statusTip())
  {
    qtAction.setStatusTip(*statusTip);
  }
  updateActionKeySequence(qtAction, tbAction);

#if defined(Q_OS_ANDROID)
  QObject::connect(&qtAction, &QAction::triggered, &qtAction, [triggerFn, &tbAction, &qtAction]() {
    QTimer::singleShot(0, &qtAction, [triggerFn, &tbAction]() { triggerFn(tbAction); });
  });
#else
  QObject::connect(
    &qtAction, &QAction::triggered, [triggerFn, &tbAction]() { triggerFn(tbAction); });
#endif

  return qtAction;
}
} // namespace

PopulateMenuResult populateMenuBar(
  ActionManager& actionManager,
  QMenuBar& qtMenuBar,
  std::unordered_map<const Action*, QAction*>& actionMap,
  const TriggerFn& triggerFn)
{
  auto result = PopulateMenuResult{};
  QMenu* currentMenu = nullptr;
#if defined(Q_OS_ANDROID)
  auto* androidMenuFilter = new AndroidMenuPressFilter{qtMenuBar, &qtMenuBar};
  qtMenuBar.installEventFilter(androidMenuFilter);
#endif

  actionManager.visitMainMenu(kdl::overload(
    [&](const MenuSeparator&) {
      contract_assert(currentMenu != nullptr);

      currentMenu->addSeparator();
    },
    [&](const MenuAction& actionItem) {
      contract_assert(currentMenu);

      auto& qtAction = findOrCreateQtAction(actionMap, actionItem.action, triggerFn);
      currentMenu->addAction(&qtAction);

      if (actionItem.entryType == MenuEntryType::Undo)
      {
        result.undoAction = &qtAction;
      }
      else if (actionItem.entryType == MenuEntryType::Redo)
      {
        result.redoAction = &qtAction;
      }
      else if (actionItem.entryType == MenuEntryType::Paste)
      {
        result.pasteAction = &qtAction;
      }
      else if (actionItem.entryType == MenuEntryType::PasteAtOriginalPosition)
      {
        result.pasteAtOriginalPositionAction = &qtAction;
      }
      else if (actionItem.entryType == MenuEntryType::Rerun)
      {
        result.rerunAction = &qtAction;
      }
    },
    [&](const auto& thisLambda, const Menu& menu) {
#if defined(Q_OS_ANDROID)
      if (currentMenu)
      {
        return;
      }
#endif
      auto* parentMenu = currentMenu;
      if (!currentMenu)
      {
        // top level menu
        currentMenu = qtMenuBar.addMenu(QString::fromStdString(menu.name));
#if defined(Q_OS_ANDROID)
        installAndroidMenuPressFilter(*currentMenu, *androidMenuFilter);
#endif
      }
      else
      {
        currentMenu = currentMenu->addMenu(QString::fromStdString(menu.name));
      }

      if (menu.entryType == MenuEntryType::RecentDocuments)
      {
        result.recentDocumentsMenu = currentMenu;
      }

      menu.visitEntries(thisLambda);
      currentMenu = parentMenu;
    }));

  return result;
}

void populateToolBar(
  ActionManager& actionManager,
  QToolBar& qtToolBar,
  std::unordered_map<const Action*, QAction*>& actionMap,
  const TriggerFn& triggerFn)
{
  actionManager.visitToolBar(kdl::overload(
    [&](const MenuSeparator&) { qtToolBar.addSeparator(); },
    [&](const MenuAction& actionItem) {
      auto& qtAction = findOrCreateQtAction(actionMap, actionItem.action, triggerFn);
      qtToolBar.addAction(&qtAction);
#if defined(Q_OS_ANDROID)
      if (auto* button = qobject_cast<QToolButton*>(qtToolBar.widgetForAction(&qtAction)))
      {
        button->installEventFilter(new AndroidToolButtonPressFilter{qtAction, button});
      }
#endif
    },
    [](const auto& thisLambda, const Menu& menu) { menu.visitEntries(thisLambda); }));
}

} // namespace tb::ui
