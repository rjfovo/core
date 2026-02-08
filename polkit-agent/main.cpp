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

#include <QApplication>
#include <PolkitQt1/Subject>
#include <QFile>
#include <QLocale>
#include <QTranslator>
#include <QThread>
#include <QDebug>

#include "polkitagentlistener.h"

int main(int argc, char *argv[])
{
    // Qt6: High DPI scaling is enabled by default, no need for AA_EnableHighDpiScaling
    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);

    // Translations
    QLocale locale;
    QString qmFilePath = QString("%1/%2.qm").arg("/usr/share/cutefish-polkit-agent/translations/").arg(locale.name());
    if (QFile::exists(qmFilePath)) {
        QTranslator *translator = new QTranslator(QApplication::instance());
        if (translator->load(qmFilePath)) {
            QApplication::installTranslator(translator);
        } else {
            translator->deleteLater();
        }
    }

    PolKitAgentListener listener;
    PolkitQt1::UnixSessionSubject session(getpid());

    // Try multiple times to register listener (in case DBus or PolicyKit is not ready yet)
    int retryCount = 0;
    const int maxRetries = 20;
    bool listenerRegistered = false;
    
    while (retryCount < maxRetries && !listenerRegistered) {
        listenerRegistered = listener.registerListener(session, QStringLiteral("/com/cutefish/PolicyKit1/AuthenticationAgent"));
        if (!listenerRegistered) {
            retryCount++;
            QThread::msleep(100); // Wait 100ms before retrying
        }
    }
    
    if (!listenerRegistered) {
        qWarning() << "Failed to register PolicyKit listener after" << maxRetries << "retries";
        return -1;
    }

    return app.exec();
}
