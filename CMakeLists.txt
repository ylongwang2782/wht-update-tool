cmake_minimum_required(VERSION 3.16)

project(wht-update-tool VERSION 1.0.0 LANGUAGES CXX)

execute_process(
    COMMAND git describe --tags --always
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 自动处理 Qt 元对象系统、UI 文件和资源文件
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# 查找 Qt 模块
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets SerialPort)

# 添加可执行文件
add_executable(${PROJECT_NAME}
    main.cpp
    widget.cpp
    Ymodem.cpp
    YmodemFileTransmit.cpp
    widget.h
    Ymodem.h
    YmodemFileTransmit.h
    widget.ui
    wht-update-tool.ico
)

target_compile_definitions(${PROJECT_NAME} PRIVATE APP_VERSION="${PROJECT_VERSION}")

# 链接 Qt 库
target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::SerialPort
)

# 设置可执行文件图标（Windows）
if(WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        WIN32_EXECUTABLE TRUE
        ICON "${CMAKE_CURRENT_SOURCE_DIR}/wht-update-tool.ico"
    )
endif()
