cmake_minimum_required(VERSION 3.24)
project(gro LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# AUTOUIC/AUTOMOC/AUTORCC let CMake invoke uic/moc/rcc automatically when
# it sees a .ui/Q_OBJECT/.qrc file in the source list. No hand-written rules.
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets)
qt_standard_project_setup()

# ----------------------------------------------------------------------------
# Dependencies (fetched on first cmake configure; no submodules required)
# ----------------------------------------------------------------------------
include(FetchContent)

# ccl: Klavins Lab's embedded interpreted language.
# Pinned to the qt-upgrade branch until that merges to master.
FetchContent_Declare(ccl
    GIT_REPOSITORY https://github.com/klavinslab/ccl.git
    GIT_TAG        qt-upgrade
)

# Chipmunk2D 7.0.3: physics engine.
FetchContent_Declare(chipmunk
    GIT_REPOSITORY https://github.com/slembcke/Chipmunk2D.git
    GIT_TAG        Chipmunk-7.0.3
)

# Chipmunk's CMakeLists.txt builds demos and several extras we don't want.
set(BUILD_DEMOS    OFF CACHE BOOL "" FORCE)
set(INSTALL_DEMOS  OFF CACHE BOOL "" FORCE)
set(INSTALL_STATIC OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED   OFF CACHE BOOL "" FORCE)
set(BUILD_STATIC   ON  CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(ccl chipmunk)

# ----------------------------------------------------------------------------
# gro executable
# ----------------------------------------------------------------------------
qt_add_executable(gro MACOSX_BUNDLE
    main.cpp
    gui.cpp
    gui.ui
    GroThread.cpp
    GroWidget.cpp
    Messages.cpp
    Programs.cpp
    Signal.cpp
    Themes.cpp
    Utility.cpp
    World.cpp
    EColi.cpp
    Gro.cpp
    GroPainter.cpp
    Cell.cpp
    reaction.cpp
    icons.qrc
)

target_link_libraries(gro PRIVATE
    Qt6::Widgets
    ccl
    chipmunk_static
)

# Old gro source uses #include "chipmunk_private.h" (unqualified). Match
# the historical include layout so the source builds without rewriting
# every include path.
target_include_directories(gro PRIVATE
    ${chipmunk_SOURCE_DIR}/include
    ${chipmunk_SOURCE_DIR}/include/chipmunk
)

# Bundle metadata.
set_target_properties(gro PROPERTIES
    MACOSX_BUNDLE_BUNDLE_NAME "gro"
    MACOSX_BUNDLE_ICON_FILE   "groicon.icns"
)
target_sources(gro PRIVATE groicon.icns)
set_source_files_properties(groicon.icns PROPERTIES
    MACOSX_PACKAGE_LOCATION "Resources"
)

# ----------------------------------------------------------------------------
# examples/ and include/ are needed at runtime, alongside the .app bundle.
# main.cpp chdirs to applicationDirPath/../../.. so they resolve there.
# ----------------------------------------------------------------------------
add_custom_command(TARGET gro POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E create_symlink
        ${CMAKE_CURRENT_SOURCE_DIR}/examples
        ${CMAKE_CURRENT_BINARY_DIR}/examples
    COMMAND ${CMAKE_COMMAND} -E create_symlink
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_BINARY_DIR}/include
    VERBATIM
)

# ----------------------------------------------------------------------------
# Bundle Qt frameworks/plugins into gro.app so it runs without env vars.
# macdeployqt copies Qt frameworks into Contents/Frameworks and rewrites
# rpaths in the binary.
# ----------------------------------------------------------------------------
if(APPLE)
    get_target_property(_qmake_path Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(_qt_bin_dir "${_qmake_path}" DIRECTORY)
    find_program(MACDEPLOYQT_EXECUTABLE macdeployqt HINTS "${_qt_bin_dir}")
    if(MACDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET gro POST_BUILD
            COMMAND "${MACDEPLOYQT_EXECUTABLE}" "$<TARGET_BUNDLE_DIR:gro>" -always-overwrite
            COMMENT "Deploying Qt into gro.app"
            VERBATIM)
    endif()
endif()
