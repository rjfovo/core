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

#include "application.h"
#include "sessionadaptor.h"

// Qt
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusVariant>
#include <QStandardPaths>
#include <QSettings>
#include <QProcess>
#include <QProcessEnvironment>
#include <QDebug>
#include <QDir>
#include <QTimer>
#include <QFile>
#include <QFileInfo>

// STL
#include <optional>

// Helper: try to get systemd user environment via dbus (org.freedesktop.systemd1.Manager Environment)
static std::optional<QStringList> getSystemdEnvironment()
{
    // Build the method call
    QDBusMessage msg = QDBusMessage::createMethodCall(
        QStringLiteral("org.freedesktop.systemd1"),
        QStringLiteral("/org/freedesktop/systemd1"),
        QStringLiteral("org.freedesktop.DBus.Properties"),
        QStringLiteral("Get"));
    msg << QStringLiteral("org.freedesktop.systemd1.Manager") << QStringLiteral("Environment");

    QDBusMessage reply = QDBusConnection::sessionBus().call(msg);
    if (reply.type() == QDBusMessage::ErrorMessage) {
        return std::nullopt;
    }

    const auto args = reply.arguments();
    if (args.isEmpty()) {
        return std::nullopt;
    }

    // The reply often contains a QDBusVariant wrapping a QVariantList or QStringList.
    QVariant first = args.at(0);
    // If it's a QDBusVariant, unwrap:
    if (first.userType() == qMetaTypeId<QDBusVariant>()) {
        QDBusVariant v = qdbus_cast<QDBusVariant>(first);
        first = v.variant();
    }

    // If it's a QStringList directly:
    if (first.canConvert<QStringList>()) {
        return first.toStringList();
    }

    // If it's a QVariantList of strings:
    if (first.type() == QVariant::List) {
        QStringList out;
        for (const QVariant &e : first.toList()) {
            if (e.canConvert<QString>())
                out << e.toString();
        }
        return out;
    }

    // Unknown format
    return std::nullopt;
}

static bool isShellVariable(const QByteArray &name)
{
    return name == "_" || name.startsWith("SHLVL");
}

static bool isSessionVariable(const QByteArray &name)
{
    // Check is variable is specific to session.
    return name == "DISPLAY" || name == "XAUTHORITY" || //
        name == "WAYLAND_DISPLAY" || name == "WAYLAND_SOCKET" || //
        name.startsWith("XDG_");
}

static void setEnvironmentVariable(const QByteArray &name, const QByteArray &value)
{
    if (qgetenv(name) != value) {
        qputenv(name, value);
    }
}

Application::Application(int &argc, char **argv)
    : QApplication(argc, argv)
    , m_processManager(new ProcessManager(this))
    , m_networkProxyManager(new NetworkProxyManager)
    , m_wayland(false)
{
    // Expose D-Bus adaptor
    new SessionAdaptor(this);

    // register DBus service/object
    QDBusConnection::sessionBus().registerService(QStringLiteral("com.cutefish.Session"));
    QDBusConnection::sessionBus().registerObject(QStringLiteral("/Session"), this);

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Cutefish Session"));
    parser.addHelpOption();

    QCommandLineOption waylandOption(QStringList() << "w" << "wayland", QStringLiteral("Wayland Mode"));
    parser.addOption(waylandOption);
    parser.process(*this);

    // If user passed --wayland or WAYLAND_DISPLAY exists, mark wayland true
    m_wayland = parser.isSet(waylandOption) || qEnvironmentVariableIsSet("WAYLAND_DISPLAY");

    // Ensure config dir exists early
    createConfigDirectory();

    // Initialize defaults and Qt/KDE specific settings
    initEnvironments();
    initKWinConfig();
    initLanguage();
    initScreenScaleFactors();
    initXResource();

    // DBus env sync must be attempted before starting desktop processes
    if (!syncDBusEnvironment()) {
        qWarning() << "Could not sync environment to dbus (dbus-update-activation-environment failed). Continuing, but desktop may fail to start.";
        // Not a hard exit here: continue, but many failures may follow.
    }

    // Import environment from systemd user manager (if available)
    importSystemdEnvrionment();

    // Clean up some env variables that we don't want inherited later
    qunsetenv("XCURSOR_THEME");
    qunsetenv("XCURSOR_SIZE");
    qunsetenv("SESSION_MANAGER");

    // Update network proxy
    if (m_networkProxyManager) m_networkProxyManager->update();

    // Defer operations that should happen after startup
    QTimer::singleShot(50, this, &Application::updateUserDirs);
    QTimer::singleShot(100, m_processManager, &ProcessManager::start);
}

bool Application::wayland() const
{
    return m_wayland;
}

void Application::launch(const QString &exec, const QStringList &args)
{
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    // Ensure our current process env is passed
    QProcess::startDetached(exec, args, QString(), nullptr);
}

void Application::launch(const QString &exec, const QString &workingDir, const QStringList &args)
{
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    QProcess::startDetached(exec, args, workingDir, nullptr);
}

void Application::initEnvironments()
{
    // Set XDG defaults if missing
    if (qEnvironmentVariableIsEmpty("XDG_DATA_HOME"))
        qputenv("XDG_DATA_HOME", QDir::home().absoluteFilePath(QStringLiteral(".local/share")).toLocal8Bit());
    if (qEnvironmentVariableIsEmpty("XDG_DESKTOP_DIR"))
        qputenv("XDG_DESKTOP_DIR", QDir::home().absoluteFilePath(QStringLiteral("Desktop")).toLocal8Bit());
    if (qEnvironmentVariableIsEmpty("XDG_CONFIG_HOME"))
        qputenv("XDG_CONFIG_HOME", QDir::home().absoluteFilePath(QStringLiteral(".config")).toLocal8Bit());
    if (qEnvironmentVariableIsEmpty("XDG_CACHE_HOME"))
        qputenv("XDG_CACHE_HOME", QDir::home().absoluteFilePath(QStringLiteral(".cache")).toLocal8Bit());
    if (qEnvironmentVariableIsEmpty("XDG_DATA_DIRS"))
        qputenv("XDG_DATA_DIRS", QByteArray("/usr/local/share/:/usr/share/"));
    if (qEnvironmentVariableIsEmpty("XDG_CONFIG_DIRS"))
        qputenv("XDG_CONFIG_DIRS", QByteArray("/etc/xdg"));

    // Desktop identification
    qputenv("DESKTOP_SESSION", "Cutefish");
    qputenv("XDG_CURRENT_DESKTOP", "Cutefish");
    qputenv("XDG_SESSION_DESKTOP", "Cutefish");

    // Qt style/platformtheme hints for Qt6
    qputenv("QT_QPA_PLATFORMTHEME", "cutefish");
    qputenv("QT_STYLE_OVERRIDE", "cutefish");

    // Force X11 (xcb) on X11 builds to avoid Qt auto-selecting wayland when you want X11.
    // If you intend to support Wayland, remove or guard this depending on m_wayland.
    // 注意：只在非Wayland模式下设置QT_QPA_PLATFORM，并且只在未设置的情况下设置
    if (!m_wayland && qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
        qputenv("QT_QPA_PLATFORM", "xcb");
    }

    // Performance tweak (kept from original)
    qputenv("QT_QPA_UPDATE_IDLE_TIME", "10");

    // disable auto screen scale unless explicitly enabled elsewhere
    qputenv("QT_AUTO_SCREEN_SCALE_FACTOR", "0");
}

void Application::initLanguage()
{
    QSettings settings(QSettings::UserScope, "cutefishos", "language");
    QString value = settings.value("language", "").toString();

    if (value.isEmpty()) {
        QFile file("/etc/locale.gen");
        if (file.open(QIODevice::ReadOnly)) {
            QStringList lines = QString(file.readAll()).split('\n');
            for (const QString &line : lines) {
                if (line.startsWith('#')) continue;
                if (line.trimmed().isEmpty()) continue;
                value = line.split(' ').first().split('.').first();
                if (!value.isEmpty()) break;
            }
        }
    }

    if (value.isEmpty())
        value = "en_US";

    settings.setValue("language", value);

    QString str = QString("%1.UTF-8").arg(value);
    const auto lcValues = {
        "LANG", "LC_NUMERIC", "LC_TIME", "LC_MONETARY", "LC_MEASUREMENT", "LC_COLLATE", "LC_CTYPE"
    };

    for (auto lc : lcValues) {
        const QString v = str;
        if (!v.isEmpty()) {
            qputenv(lc, v.toUtf8());
        }
    }

    if (!value.isEmpty()) {
        qputenv("LANGUAGE", value.toUtf8());
    }
}

void Application::initScreenScaleFactors()
{
    QSettings settings(QSettings::UserScope, "cutefishos", "theme");
    qreal scaleFactor = settings.value("PixelRatio", 1.0).toReal();

    qputenv("QT_SCREEN_SCALE_FACTORS", QByteArray::number(scaleFactor));

    // for Gtk compatibility
    if (qFloor(scaleFactor) > 1) {
        qputenv("GDK_SCALE", QByteArray::number(scaleFactor, 'g', 0));
        qputenv("GDK_DPI_SCALE", QByteArray::number(1.0 / scaleFactor, 'g', 3));
    } else {
        qputenv("GDK_SCALE", QByteArray::number(qFloor(scaleFactor), 'g', 0));
        qputenv("GDK_DPI_SCALE", QByteArray::number(qFloor(scaleFactor), 'g', 0));
    }
}

void Application::initXResource()
{
    QSettings settings(QSettings::UserScope, "cutefishos", "theme");
    qreal scaleFactor = settings.value("PixelRatio", 1.0).toReal();
    int fontDpi = qMax(1, int(96 * scaleFactor));
    QString cursorTheme = settings.value("CursorTheme", "default").toString();
    int cursorSize = qMax(1, settings.value("CursorSize", 24).toInt() * int(scaleFactor));
    int xftAntialias = settings.value("XftAntialias", 1).toBool();
    QString xftHintStyle = settings.value("XftHintStyle", "hintslight").toString();

    const QString datas = QString("Xft.dpi: %1\n"
                                  "Xcursor.theme: %2\n"
                                  "Xcursor.size: %3\n"
                                  "Xft.antialias: %4\n"
                                  "Xft.hintstyle: %5\n"
                                  "Xft.rgba: rgb")
                          .arg(fontDpi)
                          .arg(cursorTheme)
                          .arg(cursorSize)
                          .arg(xftAntialias)
                          .arg(xftHintStyle);

    QProcess p;
    p.start(QStringLiteral("xrdb"), {QStringLiteral("-quiet"), QStringLiteral("-merge"), QStringLiteral("-nocpp")});
    p.setProcessChannelMode(QProcess::ForwardedChannels);
    if (p.waitForStarted(2000)) {
        p.write(datas.toLatin1());
        p.closeWriteChannel();
        p.waitForFinished(-1);
    } else {
        qWarning() << "Could not start xrdb to set X resources";
    }

    qputenv("CUTEFISH_FONT_DPI", QByteArray::number(fontDpi));

    // Init cursor (helper)
    runSync("cupdatecursor", {cursorTheme, QString::number(cursorSize)});
}

void Application::initKWinConfig()
{
    QSettings settings(QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + "/kwinrc",
                       QSettings::IniFormat);

    settings.beginGroup("Effect-Blur");
    settings.setValue("BlurStrength", 10);
    settings.setValue("NoiseStrength", 0);
    settings.endGroup();

    settings.beginGroup("Windows");
    settings.setValue("FocusStealingPreventionLevel", 0);
    settings.setValue("HideUtilityWindowsForInactive", false);
    settings.setValue("BorderlessMaximizedWindows", false);
    settings.setValue("Placement", "Centered");
    settings.endGroup();

    settings.beginGroup("org.kde.kdecoration2");
    settings.setValue("BorderSize", "Normal");
    settings.setValue("ButtonsOnLeft", "");
    settings.setValue("ButtonsOnRight", "HIAX");
    settings.setValue("library", "org.cutefish.decoration");
    settings.setValue("theme", "");
    settings.endGroup();
}

bool Application::syncDBusEnvironment()
{
    // Attempt to synchronize a minimal set of environment variables to the DBus activation environment.
    // On modern systemd-user setups, --systemd may be undesirable because systemd-user will control this.
    // We'll try to call dbus-update-activation-environment without --systemd and explicitly pass variable names.
    QString exe = QStandardPaths::findExecutable(QStringLiteral("dbus-update-activation-environment"));
    if (exe.isEmpty()) {
        qWarning() << "dbus-update-activation-environment not found in PATH";
        return false;
    }

    // Variables we want the session activation environment to inherit.
    QStringList vars;
    vars << QStringLiteral("DISPLAY")
         << QStringLiteral("XAUTHORITY")
         << QStringLiteral("WAYLAND_DISPLAY")
         << QStringLiteral("XDG_CURRENT_DESKTOP")
         << QStringLiteral("XDG_SESSION_DESKTOP")
         << QStringLiteral("DESKTOP_SESSION")
         << QStringLiteral("QT_QPA_PLATFORMTHEME")
         << QStringLiteral("QT_STYLE_OVERRIDE")
         << QStringLiteral("LANG")
         << QStringLiteral("LANGUAGE");

    // If user explicitly wants all, we fallback to --all (but avoid --systemd)
    int rc = runSync(exe, vars);
    if (rc != 0) {
        // fallback to --all (still avoid --systemd)
        rc = runSync(exe, {QStringLiteral("--all")});
    }

    return rc == 0;
}

void Application::importSystemdEnvrionment()
{
    auto environment = getSystemdEnvironment();
    if (!environment) {
        return;
    }

    for (auto &envString : environment.value()) {
        const auto env = envString.toLocal8Bit();
        const int idx = env.indexOf('=');
        if (Q_UNLIKELY(idx <= 0)) {
            continue;
        }

        const auto name = env.left(idx);
        if (isShellVariable(name) || isSessionVariable(name)) {
            continue;
        }
        setEnvironmentVariable(name, env.mid(idx + 1));
    }
}

void Application::createConfigDirectory()
{
    const QString configDir = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);

    if (!QDir().mkpath(configDir)) {
        qWarning() << "Could not create config directory XDG_CONFIG_HOME:" << configDir;
    }
}

void Application::updateUserDirs()
{
    // This is intentionally left minimal. Optionally force xdg-user-dirs update:
    // QProcess p;
    // p.setEnvironment(QStringList() << "LC_ALL=C");
    // p.start("xdg-user-dirs-update", QStringList() << "--force");
    // p.waitForFinished(-1);
    // For now do nothing.
    Q_UNUSED(this);
}

int Application::runSync(const QString &program, const QStringList &args, const QStringList &env)
{
    QProcess p;

    // If env provided in third arg, use it as additional environment variables (NAME=VALUE)
    if (!env.isEmpty()) {
        QProcessEnvironment penv = QProcessEnvironment::systemEnvironment();
        for (const QString &e : env) {
            const int eq = e.indexOf('=');
            if (eq > 0) {
                penv.insert(e.left(eq), e.mid(eq + 1));
            } else {
                // If the env entry is not NAME=VALUE, skip inserting; it's probably not meant for environment but as arg.
            }
        }
        p.setProcessEnvironment(penv);
    } else {
        p.setProcessEnvironment(QProcessEnvironment::systemEnvironment());
    }

    p.setProcessChannelMode(QProcess::ForwardedChannels);
    p.start(program, args);
    if (!p.waitForStarted(5000)) {
        qWarning() << "Failed to start" << program << "args" << args;
        return -1;
    }
    p.waitForFinished(-1);

    if (p.exitCode()) {
        qWarning() << program << args << "exited with code" << p.exitCode();
    }

    return p.exitCode();
}
