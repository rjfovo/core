find_package(PkgConfig)
pkg_check_modules(POLKIT_AGENT REQUIRED polkit-qt6-1)
message(STATUS "polkit agent: ${POLKIT_AGENT_INCLUDE_DIRS} ${POLKIT_AGENT_LIBRARIES}")

find_package(PolkitQt6-1 REQUIRED)

# 定义源文件列表
set(SOURCE_FILES
    main.cpp
    polkitagentlistener.cpp
    dialog.cpp
)

add_executable(cutefish-polkit-agent
    ${SOURCE_FILES}
    qml.qrc
)

# 设置目标属性
target_include_directories(cutefish-polkit-agent PRIVATE
    ${POLKIT_AGENT_INCLUDE_DIRS}
)

# 修正链接库 - 使用现代CMake目标链接方式
target_link_libraries(cutefish-polkit-agent
    Qt6::Core
    Qt6::Widgets
    Qt6::Quick
    Qt6::QuickControls2
    # FishUI - 如果这是一个库，确保正确引用
    
    # PolkitQt6 库 - 使用导入的目标而不是变量
    # 注意：实际的目标名称可能需要根据 PolkitQt6-1 包提供的名称调整
    # 通常是 PolkitQt6::Core 和 PolkitQt6::Agent 之类的
    ${POLKIT_AGENT_LIBRARIES}
)

# 如果 PolkitQt6-1 提供了导入目标，应该这样链接：
# target_link_libraries(cutefish-polkit-agent
#     PolkitQt6::Core
#     PolkitQt6::Agent
# )

file(GLOB TS_FILES translations/*.ts)

# 修复 qt_add_lupdate 调用 - 添加必要的 SOURCE_TARGETS 参数
qt_add_lupdate(
    SOURCE_TARGETS cutefish-polkit-agent  # 明确指定源目标
    TS_FILES ${TS_FILES}
)

qt_add_lrelease(
    cutefish-polkit-agent  # 目标名称
    QM_FILES ${TS_FILES}
)

add_custom_target(polkit-agent-translations DEPENDS ${QM_FILES} SOURCES ${TS_FILES})
add_dependencies(cutefish-polkit-agent polkit-agent-translations)

install(TARGETS cutefish-polkit-agent
    DESTINATION ${CMAKE_INSTALL_BINDIR}
    COMPONENT Runtime
)

install(FILES
    cutefish-polkit-agent.desktop
    DESTINATION "${CMAKE_INSTALL_FULL_SYSCONFDIR}/xdg/autostart"
    COMPONENT Runtime
)

install(FILES ${QM_FILES} DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/cutefish-polkit-agent/translations)