/*
 * Copyright (C) 2021 CutefishOS Team.
 *
 * Author:     Reion Wong <reion@cutefishos.com>
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

#include "application.h"
#include "notificationsmodel.h"
#include "screenhelper.h"
#include "historymodel.h"
#include "notificationpopup.h"

#include <QCommandLineParser>
#include <QQmlApplicationEngine>
#include <QQmlContext>

#include <QDBusInterface>
#include <QDBusPendingCall>
#include <QDBusConnection>
#include <QPixmapCache>
#include <QTranslator>
#include <QFileInfo>
#include <QIcon>
#include <QDir>

#include <QDebug>

// Include generated DBus adaptor
#include "notificationadaptor.h"

Application::Application(int& argc, char** argv)
    : QGuiApplication(argc, argv)
    , m_notificationServer(NotificationServer::self())
    , m_model(NotificationsModel::self())
    , m_window(nullptr)
    , m_settings(Settings::self())
    , m_instance(false)
{
    if (QDBusConnection::sessionBus().registerService("com.cutefish.Notification")) {
        setOrganizationName("cutefishos");

        // Set icon theme for Qt6
        // In Qt6, we need to ensure icon theme is properly set
        // First set the search paths
        QStringList iconThemePaths;
        iconThemePaths << "/usr/share/icons";
        iconThemePaths << QDir::homePath() + "/.local/share/icons";
        iconThemePaths << "/usr/local/share/icons";
        QIcon::setThemeSearchPaths(iconThemePaths);
        
        // Try to set icon theme in order of preference
        QStringList preferredThemes = {"cutefish", "Crule", "Crule-dark", "breeze", "Adwaita", "hicolor"};
        QString themeSet = "hicolor"; // default fallback
        
        for (const QString &theme : preferredThemes) {
            QString themePath = QString("/usr/share/icons/%1").arg(theme);
            if (QDir(themePath).exists()) {
                themeSet = theme;
                break;
            }
        }
        
        QIcon::setThemeName(themeSet);
        qDebug() << "Icon theme set to:" << QIcon::themeName() << "from search paths:" << QIcon::themeSearchPaths();
        
        // Ensure QIcon image provider is available for QML
        // This is needed for image://icontheme/ URLs to work
        if (QIcon::themeName().isEmpty()) {
            qWarning() << "No icon theme set! image://icontheme/ URLs will not work.";
        }

        // Translations
        QLocale locale;
        QString qmFilePath = QString("%1/%2.qm").arg("/usr/share/cutefish-notificationd/translations/").arg(locale.name());
        if (QFile::exists(qmFilePath)) {
            QTranslator *translator = new QTranslator(this);
            if (translator->load(qmFilePath)) {
                installTranslator(translator);
            } else {
                translator->deleteLater();
            }
        }

        new NotificationAdaptor(this);
        QDBusConnection::sessionBus().registerObject("/Notification", this);

        qmlRegisterType<NotificationsModel>("Cutefish.Notification", 1, 0, "NotificationsModel");
        qmlRegisterType<HistoryModel>("Cutefish.Notification", 1, 0, "HistoryModel");
        qmlRegisterType<ScreenHelper>("Cutefish.Notification", 1, 0, "ScreenHelper");
        qmlRegisterType<NotificationPopup>("Cutefish.Notification", 1, 0, "NotificationPopup");

        m_instance = true;
    }
}

void Application::showWindow()
{
    if (m_window)
        m_window->open();
}

void Application::setDoNotDisturb(bool enabled)
{
    m_settings->setDoNotDisturb(enabled);
}

bool Application::doNotDisturb() const
{
    return m_settings->doNotDisturb();
}

int Application::run()
{
    if (!parseCommandLineArgs())
        return 0;

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("notificationsModel", m_model);
    engine.rootContext()->setContextProperty("Settings", m_settings);
    engine.addImportPath(QStringLiteral("/usr/share/qml"));
    engine.load(QUrl("qrc:/qml/main.qml"));

    m_window = new NotificationWindow;

    connect(m_settings, &Settings::doNotDisturbChanged, this, &Application::doNotDisturbChanged);

    return QGuiApplication::exec();
}

bool Application::parseCommandLineArgs()
{
    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Notification"));
    parser.addHelpOption();

    QCommandLineOption showOption(QStringList() << "s" << "show" << "Show dialog");
    parser.addOption(showOption);

    parser.process(arguments());

    if (m_instance) {
        QPixmapCache::setCacheLimit(2048);
    } else {
        QDBusInterface iface("com.cutefish.Notification",
                             "/Notification",
                             "com.cutefish.Notification",
                             QDBusConnection::sessionBus(), this);
        if (iface.isValid() && parser.isSet(showOption)) {
            iface.call("showWindow");
        }
    }

    return m_instance;
}
