#-------------------------------------------------
#
# Project created by QtCreator 2012-09-27T16:01:18
#
#-------------------------------------------------

QT       -= core gui

CXXFLAGS += -fast
QMAKE_CFLAGS += -std=c99

TARGET = chipmunk
TEMPLATE = lib
CONFIG += staticlib

INCLUDEPATH += ./include/chipmunk

SOURCES += \
    src/cpVect.c \
    src/cpSpaceStep.c \
    src/cpSpaceQuery.c \
    src/cpSpaceHash.c \
    src/cpSpaceComponent.c \
    src/cpSpace.c \
    src/cpShape.c \
    src/cpPolyShape.c \
    src/cpHashSet.c \
    src/cpCollision.c \
    src/cpBody.c \
    src/cpBB.c \
    src/cpArray.c \
    src/cpArbiter.c \
    src/chipmunk.c \
    src/constraints/cpSlideJoint.c \
    src/constraints/cpSimpleMotor.c \
    src/constraints/cpRotaryLimitJoint.c \
    src/constraints/cpRatchetJoint.c \
    src/constraints/cpPivotJoint.c \
    src/constraints/cpPinJoint.c \
    src/constraints/cpGrooveJoint.c \
    src/constraints/cpGearJoint.c \
    src/constraints/cpDampedSpring.c \
    src/constraints/cpDampedRotarySpring.c \
    src/constraints/cpConstraint.c

HEADERS += \
    src/prime.h \
    include/chipmunk/cpVect.h \
    include/chipmunk/cpSpaceHash.h \
    include/chipmunk/cpSpace.h \
    include/chipmunk/cpShape.h \
    include/chipmunk/cpPolyShape.h \
    include/chipmunk/cpHashSet.h \
    include/chipmunk/cpCollision.h \
    include/chipmunk/cpBody.h \
    include/chipmunk/cpBB.h \
    include/chipmunk/cpArray.h \
    include/chipmunk/cpArbiter.h \
    include/chipmunk/chipmunk.h \
    include/chipmunk/chipmunk_unsafe.h \
    include/chipmunk/chipmunk_types.h \
    include/chipmunk/chipmunk_private.h \
    include/chipmunk/chipmunk_ffi.h \
    include/chipmunk/constraints/util.h \
    include/chipmunk/constraints/cpSlideJoint.h \
    include/chipmunk/constraints/cpSimpleMotor.h \
    include/chipmunk/constraints/cpRotaryLimitJoint.h \
    include/chipmunk/constraints/cpRatchetJoint.h \
    include/chipmunk/constraints/cpPivotJoint.h \
    include/chipmunk/constraints/cpPinJoint.h \
    include/chipmunk/constraints/cpGrooveJoint.h \
    include/chipmunk/constraints/cpGearJoint.h \
    include/chipmunk/constraints/cpDampedSpring.h \
    include/chipmunk/constraints/cpDampedRotarySpring.h \
    include/chipmunk/constraints/cpConstraint.h
unix:!symbian {
    maemo5 {
        target.path = /opt/usr/lib
    } else {
        target.path = /usr/lib
    }
    INSTALLS += target
}
