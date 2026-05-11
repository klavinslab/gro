cmake_minimum_required(VERSION 3.24)
project(gro VERSION 1.0.0 LANGUAGES CXX)

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
FetchContent_Declare(ccl
    GIT_REPOSITORY https://github.com/klavinslab/ccl.git
    GIT_TAG        a4c5c96d952ae2839d3d0790607fe2879e5abeb5  # master
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

target_compile_definitions(gro PRIVATE GRO_VERSION="${PROJECT_VERSION}")

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
# Bundle examples/ and include/ into gro.app/Contents/Resources/ so the
# .app is self-contained and ships in the DMG. main.cpp chdirs to
# applicationDirPath/../Resources at startup, so the CCL lexer finds
# `include/gro.gro` relative to it.
# ----------------------------------------------------------------------------
add_custom_command(TARGET gro POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E rm -rf
        "$<TARGET_BUNDLE_CONTENT_DIR:gro>/Resources/examples"
        "$<TARGET_BUNDLE_CONTENT_DIR:gro>/Resources/include"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/examples
        "$<TARGET_BUNDLE_CONTENT_DIR:gro>/Resources/examples"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        "$<TARGET_BUNDLE_CONTENT_DIR:gro>/Resources/include"
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
        # macdeployqt invokes install_name_tool and strip via PATH. If the
        # user has MacPorts (/opt/local/bin) ahead of /usr/bin, those tools
        # are too old to read modern Mach-O ("unknown load command N"
        # errors). Prepend /usr/bin so the Xcode toolchain versions win.
        add_custom_command(TARGET gro POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E env "PATH=/usr/bin:$ENV{PATH}"
                "${MACDEPLOYQT_EXECUTABLE}" "$<TARGET_BUNDLE_DIR:gro>" -always-overwrite
            COMMENT "Deploying Qt into gro.app"
            VERBATIM)
    endif()
endif()

# ----------------------------------------------------------------------------
# Distribution: `cmake --build build --target package` produces a DMG.
# ----------------------------------------------------------------------------
if(APPLE)
    # macdeployqt already rewrote rpaths in gro.app at build time. Tell
    # CMake that the build-time rpath IS the install-time rpath so it
    # doesn't try to "fix" what's already fixed at install/cpack time.
    set_target_properties(gro PROPERTIES
        INSTALL_RPATH           "@executable_path/../Frameworks"
        BUILD_WITH_INSTALL_RPATH TRUE
        INSTALL_RPATH_USE_LINK_PATH FALSE
    )

    install(TARGETS gro BUNDLE DESTINATION .)

    set(CPACK_GENERATOR "DragNDrop")
    set(CPACK_PACKAGE_NAME "gro")
    set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
    set(CPACK_PACKAGE_FILE_NAME "gro-${PROJECT_VERSION}-mac")
    set(CPACK_DMG_VOLUME_NAME "gro ${PROJECT_VERSION}")
    set(CPACK_PACKAGE_VENDOR "Klavins Lab, University of Washington")
    include(CPack)
endif()
