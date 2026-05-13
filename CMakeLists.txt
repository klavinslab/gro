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

# Homebrew installs each Qt6 component as its own keg-only formula
# (qtbase, qtsvg, ...). Ask brew where each lives and hint find_package
# at it. find_package(Qt6 COMPONENTS Foo) does not honor CMAKE_PREFIX_PATH
# for sub-package configs once Qt6 itself is found, so we also set the
# component's _DIR variable explicitly when the package directory exists.
function(_gro_use_brew_qt_pkg brew_pkg component_dir)
    if(NOT BREW_EXECUTABLE)
        return()
    endif()
    execute_process(
        COMMAND ${BREW_EXECUTABLE} --prefix ${brew_pkg}
        OUTPUT_VARIABLE _prefix
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE _rc
    )
    if(NOT _rc EQUAL 0 OR NOT IS_DIRECTORY "${_prefix}")
        return()
    endif()
    set(CMAKE_PREFIX_PATH "${CMAKE_PREFIX_PATH};${_prefix}" PARENT_SCOPE)
    if(component_dir AND IS_DIRECTORY "${_prefix}/lib/cmake/${component_dir}")
        set(${component_dir}_DIR "${_prefix}/lib/cmake/${component_dir}" PARENT_SCOPE)
    endif()
endfunction()

if(APPLE)
    find_program(BREW_EXECUTABLE brew)
    _gro_use_brew_qt_pkg(qtbase "")
    _gro_use_brew_qt_pkg(qtsvg  Qt6Svg)

    # Homebrew's python3 isn't keg-only, but pointing Python3_ROOT_DIR
    # at it makes find_package prefer the brew Python over any
    # /Library/Frameworks/Python.framework that might also be present.
    execute_process(
        COMMAND ${BREW_EXECUTABLE} --prefix python3
        OUTPUT_VARIABLE _brew_python_prefix
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE _brew_python_rc
    )
    if(_brew_python_rc EQUAL 0 AND IS_DIRECTORY "${_brew_python_prefix}")
        set(Python3_ROOT_DIR "${_brew_python_prefix}")
    endif()
endif()

find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets Svg)
find_package(Python3 3.11 REQUIRED COMPONENTS Development Interpreter)
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

# pybind11: header-only C++/Python binding library used to embed CPython.
FetchContent_Declare(pybind11
    GIT_REPOSITORY https://github.com/pybind/pybind11.git
    GIT_TAG        v2.13.6
)

# Chipmunk's CMakeLists.txt builds demos and several extras we don't want.
set(BUILD_DEMOS    OFF CACHE BOOL "" FORCE)
set(INSTALL_DEMOS  OFF CACHE BOOL "" FORCE)
set(INSTALL_STATIC OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED   OFF CACHE BOOL "" FORCE)
set(BUILD_STATIC   ON  CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(ccl chipmunk pybind11)

# ----------------------------------------------------------------------------
# gro executable
# ----------------------------------------------------------------------------
qt_add_executable(gro MACOSX_BUNDLE
    src/main.cpp
    src/gui.cpp
    src/gui.ui
    src/GroThread.cpp
    src/GroWidget.cpp
    src/Messages.cpp
    src/Programs.cpp
    src/Signal.cpp
    src/Themes.cpp
    src/Utility.cpp
    src/World.cpp
    src/EColi.cpp
    src/Yeast.cpp
    src/Gro.cpp
    src/GroPainter.cpp
    src/Cell.cpp
    src/reaction.cpp
    src/PythonRuntime.cpp
    icons.qrc
)

target_link_libraries(gro PRIVATE
    Qt6::Widgets
    Qt6::Svg
    ccl
    chipmunk_static
    pybind11::embed
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
# include/, examples/, and python/ are siblings of gro.app at install
# time. At startup, main.cpp walks up from the executable directory
# looking for a directory that contains all three -- the source-tree
# root in dev, the install directory in production. No symlinks are
# created in the build tree; the source tree itself is the only copy.
#
# The DMG (later) ships gro.app + the three directories at its top
# level so the same search resolves to the install location.
#
# As a dev convenience, we drop a gro.app symlink at the source-tree
# root pointing at the build's bundle so the user can double-click
# from the project directory.
# ----------------------------------------------------------------------------
add_custom_command(TARGET gro POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E rm -f
        "${CMAKE_CURRENT_SOURCE_DIR}/gro.app"
    COMMAND ${CMAKE_COMMAND} -E create_symlink
        "$<TARGET_BUNDLE_DIR:gro>"
        "${CMAKE_CURRENT_SOURCE_DIR}/gro.app"
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

    # macdeployqt rewrites the binary's rpaths, which invalidates the
    # signature clang originally produced. On Apple Silicon, macOS
    # refuses to launch a Mach-O with a broken signature ("This software
    # needs to be updated"). Re-sign ad-hoc so every binary in the
    # bundle has at least a valid anonymous signature.  This does NOT
    # bypass Gatekeeper (downloaded apps still need notarization for
    # that) but it does fix the broken-signature error.
    add_custom_command(TARGET gro POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E env "PATH=/usr/bin:$ENV{PATH}"
            codesign --force --deep --sign - "$<TARGET_BUNDLE_DIR:gro>"
        COMMENT "Ad-hoc signing gro.app"
        VERBATIM)
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
