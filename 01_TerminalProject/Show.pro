CONFIG -= qt core widgets
TEMPLATE = app
QMAKE_CFLAGS=-Wall -Wextra -Werror -std=c11
TARGET = Show
INCLUDEPATH += .
LIBS += -lncursesw
SOURCES += Show.c

