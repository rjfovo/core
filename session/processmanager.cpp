/*
 * Copyright (C) 2021 CutefishOS Team.
 *
 * Author:     revenmartin <revenmartin@gmail.com>
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

#include "processmanager.h"
#include "application.h"

#include <QCoreApplication>
#include <QStandardPaths>
#include <QFileInfoList>
#include <QFileInfo>
#include <QSettings>
#include <QDebug>
#include <QTimer>
#include <QThread>
#include <QDir>

#include <QDBusInterface>
#include <QDBusPendingCall>

// KF6 兼容性处理
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <KF6/KWindowSystem/NETWM>
#else
#include <KWindowSystem/NETWM>
#endif

// Qt6 兼容性处理
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include <QX11Info>
#else
#include <QGuiApplication>
#include <qpa/qplatformnativeinterface.h>
#endif

extern "C" {
    #include <xcb/xcb.h>
    #include <xcb/xcb_ewmh.h>
}

ProcessManager::ProcessManager(Application *app, QObject *parent)
    : QObject(parent)
    , m_app(app)
    , m_wmStarted(false)
    , m_waitLoop(nullptr)
{
    qApp->installNativeEventFilter(this);
}

ProcessManager::~ProcessManager()
{
    qApp->removeNativeEventFilter(this);

    QMapIterator<QString, QProcess *> i(m_systemProcess);
    while (i.hasNext()) {
        i.next();
        QProcess *p = i.value();
        delete p;
        m_systemProcess[i.key()] = nullptr;
    }
}

void ProcessManager::start()
{
    startWindowManager();
    startDaemonProcess();
    // Start desktop process after window manager is ready
    QTimer::singleShot(1000, this, &ProcessManager::startDesktopProcess);
}

void ProcessManager::logout()
{
    QDBusInterface kwinIface("org.kde.KWin",
                             "/Session",
                             "org.kde.KWin.Session",
                             QDBusConnection::sessionBus());

    if (kwinIface.isValid()) {
        kwinIface.call("aboutToSaveSession", "cutefish");
        kwinIface.call("setState", uint(2)); // Quit
    }

    QProcess s;
    s.start("killall", QStringList() << "kglobalaccel5");
    s.waitForFinished(-1);

    QDBusInterface iface("org.freedesktop.login1",
                        "/org/freedesktop/login1/session/self",
                        "org.freedesktop.login1.Session",
                        QDBusConnection::systemBus());
    if (iface.isValid())
        iface.call("Terminate");

    QCoreApplication::exit(0);
}

void ProcessManager::startWindowManager()
{
    QProcess *wmProcess = new QProcess;

    // Discover an available KWin executable (prefer platform-specific)
    QStringList candidates;
    if (m_app->wayland()) {
        candidates << "kwin_wayland" << "kwin";
    } else {
        candidates << "kwin_x11" << "kwin";
    }

    QString wmCommand;
    for (const QString &c : candidates) {
        if (!QStandardPaths::findExecutable(c).isEmpty()) {
            wmCommand = c;
            break;
        }
    }
    if (wmCommand.isEmpty() && !candidates.isEmpty())
        wmCommand = candidates.first();

    qDebug() << "Starting window manager:" << wmCommand;

    // Prepare environment: prefer current DISPLAY/XAUTHORITY if set
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();

    if (qEnvironmentVariableIsSet("DISPLAY")) {
        env.insert("DISPLAY", qgetenv("DISPLAY"));
    }

    if (!m_app->wayland()) {
        // only force xcb in non-wayland mode if not already set
        if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
            env.insert("QT_QPA_PLATFORM", "xcb");
    }

    if (qEnvironmentVariableIsSet("XAUTHORITY")) {
        env.insert("XAUTHORITY", qgetenv("XAUTHORITY"));
        qDebug() << "Using XAUTHORITY from environment:" << qgetenv("XAUTHORITY");
    } else {
        // 查找SDDM的Xauthority文件 或 home .Xauthority
        QDir xauthDir("/run/sddm");
        QStringList xauthFiles = xauthDir.entryList(QStringList() << "xauth_*", QDir::Files);
        if (!xauthFiles.isEmpty()) {
            QString xauthFile = "/run/sddm/" + xauthFiles.first();
            env.insert("XAUTHORITY", xauthFile);
            qDebug() << "Using Xauthority file:" << xauthFile;
        } else {
            QString homeXauth = QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + "/.Xauthority";
            if (QFile::exists(homeXauth)) {
                env.insert("XAUTHORITY", homeXauth);
                qDebug() << "Using home Xauthority file:" << homeXauth;
            } else {
                qWarning() << "No Xauthority file found, not forcing XAUTHORITY";
            }
        }
    }

    wmProcess->setProcessEnvironment(env);

    // Try to start the chosen window manager; if it fails, try other candidates
    bool started = false;
    QStringList tried;
    for (const QString &candidate : candidates) {
        tried << candidate;
        QString execPath = QStandardPaths::findExecutable(candidate);
        QString program = execPath.isEmpty() ? candidate : execPath;
        qDebug() << "Attempting to start window manager program:" << program;
        wmProcess->start(program, QStringList());
        if (wmProcess->waitForStarted(5000)) {
            started = true;
            qDebug() << "Window manager started successfully:" << program;
            break;
        } else {
            qWarning() << "Failed to start window manager:" << program << "error:" << wmProcess->errorString();
        }
    }

    if (!started) {
        qWarning() << "Could not start any window manager (tried)" << tried;
        delete wmProcess;
        return;
    }
    
    if (!wmProcess->waitForStarted(5000)) {
        qWarning() << "Failed to start window manager:" << wmCommand;
        qWarning() << "Error:" << wmProcess->errorString();
        delete wmProcess;
        return;
    }
    
    qDebug() << "Window manager started successfully";
    
    if (!m_app->wayland()) {
        // 对于X11，等待窗口管理器完全启动
        QEventLoop waitLoop;
        m_waitLoop = &waitLoop;
        // 减少超时时间，避免长时间阻塞
        QTimer::singleShot(10 * 1000, &waitLoop, SLOT(quit()));
        
        // 添加一个备用检查：如果30秒后窗口管理器还没有启动，继续
        QTimer::singleShot(30000, this, [this]() {
            if (m_waitLoop && m_waitLoop->isRunning()) {
                qWarning() << "Window manager detection timeout, continuing anyway";
                m_wmStarted = true;
                m_waitLoop->quit();
            }
        });
        
        waitLoop.exec();
        m_waitLoop = nullptr;
        
        if (!m_wmStarted) {
            qWarning() << "Window manager detection failed, but continuing anyway";
            m_wmStarted = true; // 强制设置为已启动，避免阻塞
        }
    } else {
        // Wayland不需要等待
        m_wmStarted = true;
    }
    
    // 将进程添加到管理列表
    m_systemProcess.insert("windowmanager", wmProcess);
}

void ProcessManager::startDesktopProcess()
{
    // When the cutefish-settings-daemon theme module is loaded, start the desktop.
    // In the way, there will be no problem that desktop and launcher can't get wallpaper.

    QList<QPair<QString, QStringList>> list;
    // Desktop components
    list << qMakePair(QString("cutefish-notificationd"), QStringList());
    list << qMakePair(QString("cutefish-statusbar"), QStringList());
    list << qMakePair(QString("cutefish-dock"), QStringList());
    list << qMakePair(QString("cutefish-powerman"), QStringList());
    list << qMakePair(QString("cutefish-clipboard"), QStringList());
    // Always start desktop background service
    list << qMakePair(QString("cutefish-desktop-background"), QStringList());
    // File manager (started on demand, no longer runs in desktop mode)
    // Desktop background is now handled by cutefish-desktop-background

    // For CutefishOS.
    if (QFile("/usr/bin/cutefish-welcome").exists() &&
            !QFile("/run/live/medium/live/filesystem.squashfs").exists()) {
        QSettings settings("cutefishos", "login");

        if (!settings.value("Finished", false).toBool()) {
            list << qMakePair(QString("/usr/bin/cutefish-welcome"), QStringList());
        } else {
            list << qMakePair(QString("/usr/bin/cutefish-welcome"), QStringList() << "-d");
        }
    }

    for (QPair<QString, QStringList> pair : list) {
        QProcess *process = new QProcess;
        process->setProcessChannelMode(QProcess::ForwardedChannels);
        process->setProgram(pair.first);
        process->setArguments(pair.second);
        
        // 连接信号以监控进程状态
        connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                [this, pair, process](int exitCode, QProcess::ExitStatus exitStatus) {
                    if (exitCode == 0 && exitStatus == QProcess::NormalExit) {
                        qDebug() << "DE component started successfully:" << pair.first;
                    } else {
                        qWarning() << "DE component failed to start:" << pair.first 
                                   << "exit code:" << exitCode << "exit status:" << exitStatus;
                    }
                });
        
        process->start();
        
        if (process->waitForStarted(5000)) {
            qDebug() << "Load DE components: " << pair.first << pair.second << "(started successfully)";
            m_autoStartProcess.insert(pair.first, process);
        } else {
            qWarning() << "Failed to start DE component:" << pair.first 
                       << "error:" << process->errorString();
            process->deleteLater();
        }
    }

    // Auto start
    QTimer::singleShot(100, this, &ProcessManager::loadAutoStartProcess);
}

void ProcessManager::startDaemonProcess()
{
    QList<QPair<QString, QStringList>> list;
    list << qMakePair(QString("cutefish-settings-daemon"), QStringList());
    list << qMakePair(QString("cutefish-xembedsniproxy"), QStringList());
    list << qMakePair(QString("cutefish-gmenuproxy"), QStringList());
    list << qMakePair(QString("chotkeys"), QStringList());

    for (QPair<QString, QStringList> pair : list) {
        QProcess *process = new QProcess;
        process->setProcessChannelMode(QProcess::ForwardedChannels);
        process->setProgram(pair.first);
        process->setArguments(pair.second);
        
        // 连接信号以监控进程状态
        connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                [this, pair, process](int exitCode, QProcess::ExitStatus exitStatus) {
                    if (exitCode == 0 && exitStatus == QProcess::NormalExit) {
                        qDebug() << "Daemon started successfully:" << pair.first;
                    } else {
                        qWarning() << "Daemon failed to start:" << pair.first 
                                   << "exit code:" << exitCode << "exit status:" << exitStatus;
                    }
                });
        
        process->start();
        
        if (process->waitForStarted(5000)) {
            qDebug() << "Daemon started:" << pair.first;
            m_autoStartProcess.insert(pair.first, process);
        } else {
            qWarning() << "Failed to start daemon:" << pair.first 
                       << "error:" << process->errorString();
            process->deleteLater();
        }
    }
}

void ProcessManager::loadAutoStartProcess()
{
    QStringList execList;
    const QStringList dirs = QStandardPaths::locateAll(QStandardPaths::GenericConfigLocation,
                                                       QStringLiteral("autostart"),
                                                       QStandardPaths::LocateDirectory);
    for (const QString &dir : dirs) {
        const QDir d(dir);
        const QStringList fileNames = d.entryList(QStringList() << QStringLiteral("*.desktop"));
        for (const QString &file : fileNames) {
            QSettings desktop(d.absoluteFilePath(file), QSettings::IniFormat);
            desktop.beginGroup("Desktop Entry");

            if (desktop.contains("OnlyShowIn"))
                continue;

            const QString execValue = desktop.value("Exec").toString();

            // 避免冲突
            if (execValue.contains("gmenudbusmenuproxy"))
                continue;

            if (!execValue.isEmpty()) {
                execList << execValue;
            }
        }
    }

    for (const QString &exec : execList) {
        QProcess *process = new QProcess;
        process->setProgram(exec);
        
        // 连接信号以监控进程状态
        connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                [this, exec, process](int exitCode, QProcess::ExitStatus exitStatus) {
                    if (exitCode == 0 && exitStatus == QProcess::NormalExit) {
                        qDebug() << "Autostart application started successfully:" << exec;
                    } else {
                        qWarning() << "Autostart application failed to start:" << exec 
                                   << "exit code:" << exitCode << "exit status:" << exitStatus;
                    }
                });
        
        process->start();
        
        if (process->waitForStarted(5000)) {
            qDebug() << "Autostart application started:" << exec;
            m_autoStartProcess.insert(exec, process);
        } else {
            qWarning() << "Failed to start autostart application:" << exec 
                       << "error:" << process->errorString();
            process->deleteLater();
        }
    }
}

bool ProcessManager::nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result)
{
    if (eventType != "xcb_generic_event_t")
        return false;

    if (!m_wmStarted && m_waitLoop) {
        xcb_connection_t *connection = nullptr;
        
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        connection = QX11Info::connection();
#else
        if (qApp->platformName() == "xcb") {
            QPlatformNativeInterface *native = qApp->platformNativeInterface();
            if (native) {
                connection = static_cast<xcb_connection_t*>(
                    native->nativeResourceForWindow("connection", nullptr));
            }
        }
#endif

        if (connection) {
            xcb_ewmh_connection_t ewmh_conn;
            if (xcb_ewmh_init_atoms_replies(&ewmh_conn, 
                xcb_ewmh_init_atoms(connection, &ewmh_conn), nullptr)) {
                
                // 获取默认屏幕
                const xcb_setup_t *setup = xcb_get_setup(connection);
                xcb_screen_t *screen = xcb_setup_roots_iterator(setup).data;
                
                if (screen) {
                    // 方法1：检查 _NET_SUPPORTING_WM_CHECK 属性
                    xcb_get_property_cookie_t cookie = xcb_ewmh_get_supporting_wm_check(
                        &ewmh_conn, screen->root);
                    xcb_window_t wm_window;
                    if (xcb_ewmh_get_supporting_wm_check_reply(&ewmh_conn, cookie, &wm_window, nullptr)) {
                        qDebug() << "Window manager started, WM window:" << wm_window;
                        m_wmStarted = true;
                        if (m_waitLoop && m_waitLoop->isRunning())
                            m_waitLoop->exit();
                        qApp->removeNativeEventFilter(this);
                    } else {
                        // 备用方法：检查是否有任何窗口管理器属性
                        xcb_get_property_cookie_t cookie2 = xcb_ewmh_get_supporting_wm_check(
                            &ewmh_conn, screen->root);
                        xcb_generic_error_t *error = nullptr;
                        xcb_get_property_reply_t *reply = xcb_get_property_reply(connection, cookie2, &error);
                        
                        if (!error) {
                            // 即使没有WM窗口，也认为窗口管理器已启动
                            qDebug() << "Window manager check passed (no error)";
                            m_wmStarted = true;
                            if (m_waitLoop && m_waitLoop->isRunning())
                                m_waitLoop->exit();
                            qApp->removeNativeEventFilter(this);
                        }
                        
                        if (reply) free(reply);
                        if (error) free(error);
                    }
                }
                
                xcb_ewmh_connection_wipe(&ewmh_conn);
            }
        }
    }

    return false;
}
