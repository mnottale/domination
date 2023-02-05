TEMPLATE = app
TARGET = qtttest
CONFIG += console
QT = core gui widgets
SOURCES += qttest.cpp ia.cc

target.path = $$[QT_INSTALL_EXAMPLES]/winextras/iconextractor
INSTALLS += target
