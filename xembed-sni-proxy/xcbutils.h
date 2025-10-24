/*
    SPDX-FileCopyrightText: 2012, 2013 Martin Graesslin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2015 David Edmudson <davidedmundson@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <xcb/composite.h>
#include <xcb/damage.h>
#include <xcb/randr.h>
#include <xcb/shm.h>
#include <xcb/xcb.h>
#include <xcb/xcb_atom.h>
#include <xcb/xcb_event.h>

#include <QScopedPointer>
#include <QVector>

// 添加这些头文件
#include <xcb/xcb_aux.h>

/** XEMBED messages */
#define XEMBED_EMBEDDED_NOTIFY 0
#define XEMBED_WINDOW_ACTIVATE 1
#define XEMBED_WINDOW_DEACTIVATE 2
#define XEMBED_REQUEST_FOCUS 3
#define XEMBED_FOCUS_IN 4
#define XEMBED_FOCUS_OUT 5
#define XEMBED_FOCUS_NEXT 6
#define XEMBED_FOCUS_PREV 7

namespace Xcb
{
typedef xcb_window_t WindowId;

template<typename T>
using ScopedCPointer = QScopedPointer<T, QScopedPointerPodDeleter>;

class Atom
{
public:
    explicit Atom(const QByteArray &name, bool onlyIfExists = false, xcb_connection_t *c = nullptr)
        : m_connection(c)
        , m_retrieved(false)
        , m_atom(XCB_ATOM_NONE)
        , m_name(name)
    {
        // 如果没有提供连接，尝试获取默认连接
        if (!m_connection) {
            // 这里需要确保已经有活动的连接
            // 在实际使用中，建议总是传入连接
            return;
        }
        m_cookie = xcb_intern_atom_unchecked(m_connection, onlyIfExists, name.length(), name.constData());
    }
    Atom() = delete;
    Atom(const Atom &) = delete;

    ~Atom()
    {
        if (!m_retrieved && m_cookie.sequence && m_connection) {
            xcb_discard_reply(m_connection, m_cookie.sequence);
        }
    }

    operator xcb_atom_t() const
    {
        (const_cast<Atom *>(this))->getReply();
        return m_atom;
    }
    bool isValid()
    {
        getReply();
        return m_atom != XCB_ATOM_NONE;
    }
    bool isValid() const
    {
        (const_cast<Atom *>(this))->getReply();
        return m_atom != XCB_ATOM_NONE;
    }

    inline const QByteArray &name() const
    {
        return m_name;
    }

    // 设置连接的方法
    void setConnection(xcb_connection_t *conn)
    {
        m_connection = conn;
        if (m_connection && !m_name.isEmpty()) {
            m_cookie = xcb_intern_atom_unchecked(m_connection, false, m_name.length(), m_name.constData());
            m_retrieved = false;
        }
    }

private:
    void getReply()
    {
        if (m_retrieved || !m_cookie.sequence || !m_connection) {
            return;
        }
        ScopedCPointer<xcb_intern_atom_reply_t> reply(xcb_intern_atom_reply(m_connection, m_cookie, nullptr));
        if (!reply.isNull()) {
            m_atom = reply->atom;
        }
        m_retrieved = true;
    }
    xcb_connection_t *m_connection;
    bool m_retrieved;
    xcb_intern_atom_cookie_t m_cookie;
    xcb_atom_t m_atom;
    QByteArray m_name;
};

class Atoms
{
public:
    Atoms(xcb_connection_t *connection = nullptr, int screen = 0)
        : m_connection(connection)
        , m_screen(screen)
        , xembedAtom("_XEMBED", false, connection)
        , selectionAtom(getSelectionAtomName(screen), false, connection)
        , opcodeAtom("_NET_SYSTEM_TRAY_OPCODE", false, connection)
        , messageData("_NET_SYSTEM_TRAY_MESSAGE_DATA", false, connection)
        , visualAtom("_NET_SYSTEM_TRAY_VISUAL", false, connection)
        // 添加缺失的原子
        , netWmNameAtom("_NET_WM_NAME", false, connection)
        , utf8StringAtom("UTF8_STRING", false, connection)
        , wmNameAtom("WM_NAME", false, connection)
    {
    }

    void setConnection(xcb_connection_t *connection, int screen = 0)
    {
        m_connection = connection;
        m_screen = screen;
        
        xembedAtom.setConnection(connection);
        selectionAtom = Atom(getSelectionAtomName(screen), false, connection);
        opcodeAtom.setConnection(connection);
        messageData.setConnection(connection);
        visualAtom.setConnection(connection);
        // 为新原子设置连接
        netWmNameAtom.setConnection(connection);
        utf8StringAtom.setConnection(connection);
        wmNameAtom.setConnection(connection);
    }

    Atom xembedAtom;
    Atom selectionAtom;
    Atom opcodeAtom;
    Atom messageData;
    Atom visualAtom;
    // 添加缺失的原子成员
    Atom netWmNameAtom;
    Atom utf8StringAtom;
    Atom wmNameAtom;

private:
    QByteArray getSelectionAtomName(int screen) const
    {
        return QByteArray("_NET_SYSTEM_TRAY_S") + QByteArray::number(screen);
    }

    xcb_connection_t *m_connection;
    int m_screen;
};

extern Atoms *atoms;

} // namespace Xcb