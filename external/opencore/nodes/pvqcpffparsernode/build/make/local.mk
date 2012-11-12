# Get the current local path as the first operation
LOCAL_PATH := $(call get_makefile_dir)

# Clear out the variables used in the local makefiles
include $(MK)/clear.mk

TARGET := pvqcpffparsernode


XCXXFLAGS += $(FLAG_COMPILE_WARNINGS_AS_ERRORS)







SRCDIR := ../../src
INCSRCDIR := ../../include

SRCS := pvmf_qcpffparser_node.cpp pvmf_qcpffparser_port.cpp pvmf_qcpffparser_factory.cpp

HDRS := pvmf_qcpffparser_factory.h \
	pvmf_qcpffparser_registry_factory.h \
	pvmf_qcpffparser_events.h



include $(MK)/library.mk
