project(cutefish-session)
set(TARGET cutefish-session)

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

find_package(KF6WindowSystem)

add_executable(${TARGET} ${SOURCES} ${DBUS_SOURCES})
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

# 添加包含路径
target_include_directories(cutefish-session PRIVATE
    ${XCB_INCLUDE_DIRS}
    ${XCB_EWMH_INCLUDE_DIRS}
)

install(TARGETS ${TARGET} DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES cutefish-xsession.desktop DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/xsessions/)
