project(cutefish-shutdown)

add_executable(cutefish-shutdown
    main.cpp
    actions.cpp
    qml.qrc
)

target_link_libraries(cutefish-shutdown
        Qt6::Core
        Qt6::Widgets
        Qt6::Quick
        Qt6::QuickControls2
        Qt6::DBus
)

file(GLOB TS_FILES translations/*.ts)
# 修复 qt_add_lupdate 调用 - 添加必要的 SOURCE_TARGETS 参数
qt_add_lupdate(
    SOURCE_TARGETS cutefish-shutdown  # 明确指定源目标
    TS_FILES ${TS_FILES}
)

qt_add_lrelease(
    cutefish-shutdown  # 目标名称
    QM_FILES ${TS_FILES}
)

add_custom_target(shutdown-translations DEPENDS ${QM_FILES} SOURCES ${TS_FILES})
add_dependencies(cutefish-shutdown shutdown-translations)
install(FILES ${QM_FILES} DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/cutefish-shutdown/translations)

install(TARGETS cutefish-shutdown RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
