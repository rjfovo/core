#include <QGuiApplication>
#include <QFile>
#include <QDebug>
#include <QSettings>
#include <QStandardPaths>

#include <X11/X.h>
#include <X11/Xcursor/Xcursor.h>

// 注意：在Qt6中，我们使用 QNativeInterface::QX11Application 来获取X11连接
#include <qpa/qplatformnativeinterface.h>

inline void applyTheme(const QString &theme, int size)
{
    // Qt6 方式获取 Display
    Display *display = nullptr;
    
    // 检查是否在X11平台运行
    if (qApp->platformName() == "xcb") {
        QPlatformNativeInterface *native = qApp->platformNativeInterface();
        if (native) {
            display = static_cast<Display*>(
                native->nativeResourceForWindow("display", nullptr));
        }
    }

    if (!display) {
        qWarning() << "Unable to get X11 display";
        return;
    }

    if (!theme.isEmpty())
        XcursorSetTheme(display, QFile::encodeName(theme).constData());

    if (size > 0)
        XcursorSetDefaultSize(display, size);

    Cursor handle = XcursorLibraryLoadCursor(display, "left_ptr");
    XDefineCursor(display, DefaultRootWindow(display), handle);
    XFreeCursor(display, handle);
    XFlush(display);

    // For KWin
    QSettings settings(QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + "/kcminputrc",
                       QSettings::IniFormat);
    settings.beginGroup("Mouse");
    settings.setValue("cursorTheme", theme);
    settings.setValue("cursorSize", size);
    settings.endGroup();
    settings.sync();
}

int main(int argc, char *argv[])
{
    QGuiApplication::setDesktopSettingsAware(false);
    QGuiApplication a(argc, argv);

    if (argc != 3)
        return 1;

    // Qt6 方式检查平台
    if (a.platformName() != "xcb")
        return 2;

    QString theme = QFile::decodeName(argv[1]);
    QString size = QFile::decodeName(argv[2]);

    applyTheme(theme, size.toInt());

    return 0;
}