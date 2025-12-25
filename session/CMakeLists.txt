cmake_minimum_required(VERSION 3.16)
project(cutefish-session)
set(TARGET cutefish-session)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Quick DBus)
find_package(KF6WindowSystem REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(XCB REQUIRED xcb)
pkg_check_modules(XCB_EWMH REQUIRED xcb-ewmh)

# 打印调试信息，确认找到的库
message(STATUS "XCB libraries: ${XCB_LIBRARIES}")
message(STATUS "XCB EWMH libraries: ${XCB_EWMH_LIBRARIES}")
message(STATUS "XCB EWMH include dirs: ${XCB_EWMH_INCLUDE_DIRS}")

set(SOURCES
    application.cpp
    main.cpp
    process.cpp
    processmanager.cpp
    networkproxymanager.cpp

    powermanager/power.cpp
    powermanager/powerproviders.cpp
)

qt6_add_dbus_adaptor(DBUS_SOURCES
                     com.cutefish.Session.xml
                     application.h Application
                     sessionadaptor SessionAdaptor)
set_source_files_properties(${DBUS_SOURCES} PROPERTIES SKIP_AUTOGEN ON)

add_executable(${TARGET} ${SOURCES} ${DBUS_SOURCES})

# 启用AUTOMOC
set_target_properties(${TARGET} PROPERTIES
    AUTOMOC ON
    AUTORCC ON
    AUTOUIC ON
)

# 添加包含路径
target_include_directories(${TARGET} PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${XCB_INCLUDE_DIRS}
    ${XCB_EWMH_INCLUDE_DIRS}
)

target_link_libraries(${TARGET}
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Quick
        Qt6::DBus
        KF6::WindowSystem
        Qt6::CorePrivate  # 这会自动处理私有头文件路径
        Qt6::GuiPrivate   # 这会自动处理私有头文件路径
        ${XCB_LIBRARIES}
        ${XCB_EWMH_LIBRARIES}
)

install(TARGETS ${TARGET} DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES cutefish-xsession.desktop DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/xsessions/)
