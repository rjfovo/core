project(cutefish-xembedsniproxy)

add_definitions(-DQT_NO_CAST_TO_ASCII
-DQT_NO_CAST_FROM_ASCII
-DQT_NO_URL_CAST_FROM_STRING
-DQT_NO_CAST_FROM_BYTEARRAY)

find_package(X11)
set_package_properties(X11 PROPERTIES DESCRIPTION "X11 libraries"
                        URL "http://www.x.org"
                        TYPE OPTIONAL
                        PURPOSE "Required for building the X11 based workspace")

if(X11_FOUND)
  find_package(XCB MODULE REQUIRED COMPONENTS XCB RANDR)
  set_package_properties(XCB PROPERTIES TYPE REQUIRED)
  if(NOT X11_SM_FOUND)
    message(FATAL_ERROR "\nThe X11 Session Management (SM) development package could not be found.\nPlease install libSM.\n")
  endif(NOT X11_SM_FOUND)
  #find_package(Qt6 ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS X11Extras)
endif()

if(X11_FOUND AND XCB_XCB_FOUND)
  set(HAVE_X11 1)
endif()

find_package(XCB
    REQUIRED COMPONENTS
        XCB
        XFIXES
        DAMAGE
        COMPOSITE
        RANDR
        SHM
        UTIL
        IMAGE
)

# 查找 X11 和 XCB 相关库
pkg_check_modules(X11 REQUIRED x11)
pkg_check_modules(XCB REQUIRED xcb)
pkg_check_modules(X11_XCB REQUIRED x11-xcb)  # 专门用于 XGetXCBConnection
pkg_check_modules(XCB_UTIL REQUIRED xcb-util)

find_package(KF6WindowSystem)

set(XCB_LIBS
    XCB::XCB
    XCB::XFIXES
    XCB::DAMAGE
    XCB::COMPOSITE
    XCB::RANDR
    XCB::SHM
    XCB::UTIL
    XCB::IMAGE
)

set(XEMBED_SNI_PROXY_SOURCES
    main.cpp
    fdoselectionmanager.cpp
    snidbus.cpp
    sniproxy.cpp
    debug.cpp
    xtestsender.cpp
    xcb_connection.cpp
)

qt6_add_dbus_adaptor(DBUS_SOURCES org.kde.StatusNotifierItem.xml
                     sniproxy.h SNIProxy)
set(statusnotifierwatcher_xml org.kde.StatusNotifierWatcher.xml)
qt6_add_dbus_interface(DBUS_SOURCES ${statusnotifierwatcher_xml} statusnotifierwatcher_interface)

set_source_files_properties(${DBUS_SOURCES} PROPERTIES SKIP_AUTOGEN ON)

add_executable(cutefish-xembedsniproxy ${XEMBED_SNI_PROXY_SOURCES} ${DBUS_SOURCES})
set_package_properties(XCB PROPERTIES TYPE REQUIRED)
target_link_libraries(cutefish-xembedsniproxy
    Qt6::Core
    Qt6::DBus
    KF6::WindowSystem
    ${XCB_LIBS}
    ${X11_XTest_LIB}
    ${X11_LIBRARIES}
    ${XCB_LIBRARIES}
    ${X11_XCB_LIBRARIES}  # 添加这个
    ${XCB_UTIL_LIBRARIES}
)

install(TARGETS cutefish-xembedsniproxy DESTINATION ${CMAKE_INSTALL_BINDIR})
