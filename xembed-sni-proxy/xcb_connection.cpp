// xcbconnection.cpp
#include "xcb_connection.h"
#include <X11/Xlib-xcb.h>
#include <stdexcept>

XcbConnection& XcbConnection::instance()
{
    static XcbConnection instance;
    return instance;
}

XcbConnection::XcbConnection()
    : m_connection(nullptr), m_screen(nullptr), m_xlibDisplay(nullptr)
{
    // 首先尝试通过 Xlib 获取显示，这样我们可以同时获得 Xlib Display 和 XCB 连接
    m_xlibDisplay = XOpenDisplay(nullptr);
    if (!m_xlibDisplay) {
        throw std::runtime_error("无法打开 X11 显示");
    }
    
    m_connection = XGetXCBConnection(m_xlibDisplay);
    if (!m_connection || xcb_connection_has_error(m_connection)) {
        if (m_xlibDisplay) XCloseDisplay(m_xlibDisplay);
        throw std::runtime_error("无法获取 XCB 连接");
    }
    
    // 获取屏幕
    const xcb_setup_t* setup = xcb_get_setup(m_connection);
    xcb_screen_iterator_t iter = xcb_setup_roots_iterator(setup);
    m_screen = iter.data;
    
    if (!m_screen) {
        XCloseDisplay(m_xlibDisplay);
        throw std::runtime_error("无法获取 XCB 屏幕");
    }
}

XcbConnection::~XcbConnection()
{
    if (m_xlibDisplay) {
        XCloseDisplay(m_xlibDisplay);
    }
    // 注意：不要关闭 m_connection，因为它属于 Xlib Display
}