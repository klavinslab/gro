#-------------------------------------------------
#
# Project created by QtCreator 2012-05-23T09:21:54
#
#-------------------------------------------------

# uncomment this line to compile grong (the no gui version)
# CONFIG += nogui

CONFIG += warn_off

contains ( CONFIG, nogui ) {
  QMAKE_CXXFLAGS += -DNOGUI -std=c99
}

win32 {
  QMAKE_CXXFLAGS += -O3 -DWIN -DNDEBUG
  contains ( CONFIG, nogui ) {
    CONFIG += console
  }
  CONFIG -= app_bundle
}

macx {
  QMAKE_CXXFLAGS += -fast
}

contains ( CONFIG, nogui ) {
  QT -= core gui
  TARGET = grong
  TEMPLATE = app
} else {
  QT += core gui widgets
  TARGET = gro
  TEMPLATE = app
}

ICON = groicon.icns

SOURCES += main.cpp\
    gui.cpp \
    GroThread.cpp \
    GroWidget.cpp \
    Messages.cpp \
    Programs.cpp \
    Signal.cpp \
    Themes.cpp \
    Utility.cpp \
    World.cpp \
    EColi.cpp \
    Gro.cpp \
    GroPainter.cpp \
    Cell.cpp \
    reaction.cpp

HEADERS  += gui.h \
    GroThread.h \
    GroWidget.h \
    Defines.h \
    Programs.h \
    Utility.h \
    Micro.h \
    GroPainter.h \
    Theme.h \
    EColi.h \
    Cell.h \
    ui_gui.h

contains ( CONFIG, nogui ) {
  SOURCES -= GroThread.cpp GroWidget.cpp GroPainter.cpp gui.cpp Themes.cpp
  HEADERS -= gui.h GroThread.h GroWidget.h GroPainter.h
}

!contains ( CONFIG, nogui ) {
  FORMS    += gui.ui
}

macx {
  LIBS += -L../build-ccl-Desktop_Qt_5_0_2_clang_64bit-Release/ -lccl -L../chipmunk/src -lchip
  PRE_TARGETDEPS += ../build-ccl-Desktop_Qt_5_0_2_clang_64bit-Release/libccl.a
  DEPENDPATH += ../chipmunk/
  INCLUDEPATH += ../ccl/ ../chipmunk/include/chipmunk/
}

OTHER_FILES += \
    examples/wave.gro \
    examples/skin.gro \
    examples/morphogenesis.gro \
    examples/inducer.gro \
    examples/growth.gro \
    examples/gfp.gro \
    examples/game.gro \
    examples/foreach.gro \
    examples/edge.gro \
    examples/dilution.gro \
    examples/chemotaxis.gro \
    examples/bandpass.gro \
    include/gro.gro \
    include/standard.gro \
    icons/stop.png \
    icons/step.png \
    icons/start.png \
    icons/reload.png \
    icons/open.png \
    examples/signal_demo.gro \
    examples/maptocells.gro \
    examples/game.gro \
    changelog.txt \
    error.tiff \
    examples/signal_grid.gro \
    examples/coupled_oscillator.gro \
    examples/spots.gro \
    examples/spatial_oscillations.gro \
    examples/symbiosis.gro \
    LICENSE.txt

!contains ( CONFIG, nogui ) {
  RESOURCES += icons.qrc
}

QMAKE_CLEAN += Makefile moc_GroPainter.cpp
