set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2)

qt_add_executable(ladybird main.cpp)

target_sources(ladybird PRIVATE
    Application.cpp
    WebContentView.cpp
    ladybird.qrc
    ${LADYBIRD_SOURCE_DIR}/UI/Qt/EventLoopImplementationQt.cpp
    ${LADYBIRD_SOURCE_DIR}/UI/Qt/EventLoopImplementationQtEventTarget.cpp
    ${LADYBIRD_SOURCE_DIR}/UI/Qt/StringUtils.cpp
)

if (APPLE)
    target_sources(ladybird PRIVATE WebContentViewPaintMac.mm)
    set_source_files_properties(WebContentViewPaintMac.mm PROPERTIES
        COMPILE_FLAGS "-fobjc-arc"
    )
    target_link_libraries(ladybird PRIVATE "-framework Metal" "-framework IOSurface" "-framework QuartzCore")
elseif (LINUX)
    target_sources(ladybird PRIVATE WebContentViewPaintLinux.cpp)
    find_package(OpenGL REQUIRED COMPONENTS EGL)
    target_link_libraries(ladybird PRIVATE OpenGL::EGL)
endif()

target_link_libraries(ladybird PRIVATE Qt::Core Qt::Gui Qt::Qml Qt::Quick Qt::QuickControls2)
target_include_directories(ladybird PRIVATE ${LADYBIRD_SOURCE_DIR})

create_ladybird_bundle(ladybird)
