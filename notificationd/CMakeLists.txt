cmake_minimum_required(VERSION 3.16)
project(cutefish-notificationd)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Core DBus Quick Widgets LinguistTools)
find_package(KF6WindowSystem)

set(SRCS main.cpp
         application.cpp
         datehelper.cpp
         screenhelper.cpp
         notificationsmodel.cpp
         notificationserver.cpp
         notification.cpp
         notificationpopup.cpp
         notificationwindow.cpp
         historymodel.cpp
         settings.cpp
         utils.cpp
         dbus/notificationsadaptor.cpp
         resources.qrc
)

# for Ubuntu
# qt_add_dbus_adaptor(SRCS org.freedesktop.Notifications.xml notificationsserver.h NotificationServer)

qt6_add_dbus_adaptor(DBUS_SOURCES com.cutefish.Notification.xml application.h Application)
set_source_files_properties(${DBUS_SOURCES} PROPERTIES SKIP_AUTOGEN ON)

# Find FishUI library
find_library(FISHUI_LIBRARY FishUI
    PATHS /usr/lib/x86_64-linux-gnu
          /usr/local/lib
    NO_DEFAULT_PATH
)

add_executable(cutefish-notificationd ${SRCS} ${DBUS_SOURCES})

# Enable AUTOMOC for Qt classes
set_target_properties(cutefish-notificationd PROPERTIES
    AUTOMOC ON
)

# Add build directory to include path for generated DBus adaptor
target_include_directories(cutefish-notificationd PRIVATE 
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(cutefish-notificationd
    Qt6::Core
    Qt6::DBus
    Qt6::Quick
    Qt6::Widgets
    KF6::WindowSystem
    ${FISHUI_LIBRARY}
)

install(TARGETS cutefish-notificationd
    DESTINATION /usr/bin
    COMPONENT Runtime
)

file(GLOB TS_FILES translations/*.ts)
qt_add_lupdate(
    SOURCE_TARGETS cutefish-notificationd
    TS_FILES ${TS_FILES}
)

qt_add_lrelease(
    cutefish-notificationd
    QM_FILES ${TS_FILES}
)
add_custom_target(notificationd_translations DEPENDS ${QM_FILES} SOURCES ${TS_FILES})
add_dependencies(cutefish-notificationd notificationd_translations)

install(FILES ${QM_FILES} DESTINATION /usr/share/cutefish-notificationd/translations)
