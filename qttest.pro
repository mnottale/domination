TEMPLATE = app
TARGET = qtttest
CONFIG += console
QT = core gui widgets
SOURCES += qttest.cpp

target.path = $$[QT_INSTALL_EXAMPLES]/winextras/iconextractor
INSTALLS += target
