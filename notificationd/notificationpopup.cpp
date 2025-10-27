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

#include "notificationpopup.h"

#include <QQmlContext>
#include <QWindow>

NotificationPopup::NotificationPopup(QQuickView *parent)
    : QQuickView(parent)
{
    installEventFilter(this);

    setResizeMode(QQuickView::SizeRootObjectToView);
    setColor(Qt::transparent);
    
    // 在构造函数中一次性设置所有窗口标志
    setFlags(Qt::Tool | 
             Qt::FramelessWindowHint | 
             Qt::WindowDoesNotAcceptFocus |
             Qt::WindowStaysOnTopHint);
}

bool NotificationPopup::eventFilter(QObject *object, QEvent *event)
{
    // 如果不需要特殊处理 Show 事件，可以完全移除这个条件
    // 或者只处理其他类型的事件
    return QObject::eventFilter(object, event);
}