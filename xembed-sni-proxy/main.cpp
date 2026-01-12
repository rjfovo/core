/*
    Main
    SPDX-FileCopyrightText: 2015 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: LGPL-2.1-or-later
*/

#include <QGuiApplication>
#include <QSessionManager>

#include "fdoselectionmanager.h"

#include "debug.h"
#include "snidbus.h"
#include "xcbutils.h"
#include "xcb_connection.h"

#include <QDBusMetaType>

#include <KWindowSystem>

namespace Xcb
{
Xcb::Atoms *atoms;
}

int main(int argc, char **argv)
{
    // the whole point of this is to interact with X, if we are in any other session, force trying to connect to X
    // if the QPA can't load xcb, this app is useless anyway.
    qputenv("QT_QPA_PLATFORM", "xcb");

    QGuiApplication::setDesktopSettingsAware(false);

    QGuiApplication app(argc, argv);

    if (!KWindowSystem::isPlatformX11()) {
        qFatal("xembed-sni-proxy is only useful XCB. Aborting");
    }

    auto disableSessionManagement = [](QSessionManager &sm) {
        sm.setRestartHint(QSessionManager::RestartNever);
    };
    QObject::connect(&app, &QGuiApplication::commitDataRequest, disableSessionManagement);
    QObject::connect(&app, &QGuiApplication::saveStateRequest, disableSessionManagement);

    app.setQuitOnLastWindowClosed(false);

    qDBusRegisterMetaType<KDbusImageStruct>();
    qDBusRegisterMetaType<KDbusImageVector>();
    qDBusRegisterMetaType<KDbusToolTipStruct>();

    // 先创建FdoSelectionManager，它会初始化XCB连接
    FdoSelectionManager manager;
    
    // 现在我们可以初始化atoms，使用manager的连接
    if (manager.connection()) {
        Xcb::atoms = new Xcb::Atoms(manager.connection(), manager.screenNumber());
    } else {
        qFatal("Failed to get XCB connection from FdoSelectionManager");
    }

    // 确保在事件循环开始前，atoms已经初始化
    // FdoSelectionManager::init() 将通过 QTimer::singleShot 在事件循环中调用
    // 此时 Xcb::atoms 应该已经可用

    auto rc = app.exec();

    delete Xcb::atoms;
    Xcb::atoms = nullptr;
    return rc;
}
