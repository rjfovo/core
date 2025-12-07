#include "touchpadmanager.h"
#include "touchpadadaptor.h"

#include <QDebug>

TouchpadManager::TouchpadManager(QObject *parent)
    : QObject(parent)
    , m_backend(XlibBackend::initialize())
{
    // init dbus
    new TouchpadAdaptor(this);
    QDBusConnection::sessionBus().registerObject(QStringLiteral("/Touchpad"), this);

    if (m_backend) {
        m_backend->getConfig();
        m_backend->applyConfig();
    }
}

bool TouchpadManager::available() const
{
    return m_backend ? m_backend->isTouchpadAvailable() : false;
}

bool TouchpadManager::enabled() const
{
    return m_backend ? m_backend->isTouchpadEnabled() : false;
}

void TouchpadManager::setEnabled(bool enabled)
{
    if (m_backend) {
        m_backend->setTouchpadEnabled(enabled);
        m_backend->applyConfig();
    }
}

bool TouchpadManager::tapToClick() const
{
    return m_backend ? m_backend->tapToClick() : false;
}

void TouchpadManager::setTapToClick(bool value)
{
    if (m_backend) {
        m_backend->setTapToClick(value);
        m_backend->applyConfig();
    }
}

bool TouchpadManager::naturalScroll() const
{
    return m_backend ? m_backend->naturalScroll() : false;
}

void TouchpadManager::setNaturalScroll(bool naturalScroll)
{
    if (m_backend) {
        m_backend->setNaturalScroll(naturalScroll);
        m_backend->applyConfig();
    }
}

qreal TouchpadManager::pointerAcceleration() const
{
    return m_backend ? m_backend->pointerAcceleration() : 1.0;
}

void TouchpadManager::setPointerAcceleration(qreal value)
{
    qDebug() << value;
    if (m_backend) {
        m_backend->setPointerAcceleration(value);
        m_backend->applyConfig();
    }
}
