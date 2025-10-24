// xcbconnection.h
#ifndef XCBCONNECTION_H
#define XCBCONNECTION_H

#include <xcb/xcb.h>
#include <X11/Xlib.h>  // 添加这个头文件
#include <memory>

class XcbConnection
{
public:
    static XcbConnection& instance();
    
    xcb_connection_t* connection() const { return m_connection; }
    xcb_screen_t* screen() const { return m_screen; }
    xcb_window_t rootWindow() const { return m_screen->root; }
    Display* xlibDisplay() const { return m_xlibDisplay; }
    
    bool isValid() const { return m_connection && !xcb_connection_has_error(m_connection); }

private:
    XcbConnection();
    ~XcbConnection();
    
    xcb_connection_t* m_connection;
    xcb_screen_t* m_screen;
    Display* m_xlibDisplay;
};

#endif // XCBCONNECTION_H