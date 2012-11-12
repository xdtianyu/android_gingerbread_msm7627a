# Get the current local path as the first operation
LOCAL_PATH := $(call get_makefile_dir)

# Clear out the variables used in the local makefiles
include $(MK)/clear.mk

TARGET := pvqcpparser


XCXXFLAGS += $(FLAG_COMPILE_WARNINGS_AS_ERRORS)








SRCDIR := ../../src
INCSRCDIR := ../../include

SRCS := qcpfileparser.cpp \
        iqcpff.cpp \
        qcpparser.cpp \
        qcputils.cpp

HDRS := qcpfileparser.h \
        iqcpff.h \
        qcpparser.h \
        qcputils.h



include $(MK)/library.mk
