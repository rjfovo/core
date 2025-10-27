/*
 * Copyright (C) 2021 CutefishOS Team.
 *
 * Author:     Kate Leet <kate@cutefishos.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "notificationwindow.h"
#include "notificationsmodel.h"
#include "historymodel.h"

#include <QQmlContext>

// 注释掉或移除 KWindowSystem 相关头文件
// #include <KWindowSystem>
// #include <KWindowEffects>

NotificationWindow::NotificationWindow(QQuickView *parent)
    : QQuickView(parent)
{
    installEventFilter(this);

    // 使用 Qt 原生方式设置窗口属性
    setFlags(Qt::Popup | Qt::FramelessWindowHint | Qt::WindowDoesNotAcceptFocus);
    setResizeMode(QQuickView::SizeRootObjectToView);
    setColor(Qt::transparent);

    rootContext()->setContextProperty("NotificationDialog", this);
    rootContext()->setContextProperty("notificationsModel", NotificationsModel::self());
    rootContext()->setContextProperty("historyModel", HistoryModel::self());

    // 注释掉 KWindowEffects 相关代码
    // KWindowEffects::slideWindow(winId(), KWindowEffects::RightEdge);
    
    setSource(QUrl("qrc:/qml/NotificationWindow.qml"));
    setVisible(false);
}

void NotificationWindow::open()
{
    setVisible(true);
    setMouseGrabEnabled(true);
    setKeyboardGrabEnabled(true);
}

bool NotificationWindow::eventFilter(QObject *object, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonPress) {
        if (QWindow *w = qobject_cast<QWindow*>(object)) {
            // 修复弃用的 globalPos() 调用
            if (!w->geometry().contains(static_cast<QMouseEvent*>(event)->globalPosition().toPoint())) {
                QQuickView::setVisible(false);
            }
        }
    } else if (event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Escape) {
            QQuickView::setVisible(false);
        }
    } else if (event->type() == QEvent::Show) {
        // 移除 KWindowSystem 调用，使用 Qt 原生方式
        // KWindowSystem::setState(winId(), NET::SkipTaskbar | NET::SkipPager | NET::SkipSwitcher);
        
        // 确保窗口不会出现在任务栏等地方
        setFlags(flags() | Qt::Tool | Qt::FramelessWindowHint | Qt::WindowDoesNotAcceptFocus);
        
        HistoryModel::self()->updateTime();
    } else if (event->type() == QEvent::Hide) {
        setMouseGrabEnabled(false);
        setKeyboardGrabEnabled(false);
    }

    return QObject::eventFilter(object, event);
}