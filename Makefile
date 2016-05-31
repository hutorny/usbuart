# Makefile - make script to build USBUART Library
#
# Copyright (C) 2016 Eugene Hutorny <eugene@hutorny.in.ua>
#
# This file is part of USBUART Library. http://usbuart.info
#
# The USBUART Library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public License v2
# as published by the Free Software Foundation;
#
# The USBUART Library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with the USBUART Library; if not, see
# <http://www.gnu.org/licenses/gpl-2.0.html>.

# This makefile builds unit tests for cojson
# It is not intended to build any other applications

MAKEFLAGS += --no-builtin-rules
TARGET-DIR := bin
BUILD-DIR ?= /tmp/usbuart
BOLD:=$(shell tput bold)
NORM:=$(shell tput sgr0)

CC  := $(if $(V),,@)$(PREFIX)gcc$(SUFFIX)
CXX := $(if $(V),,@)$(PREFIX)g++$(SUFFIX)
LD  := $(if $(V),,@)$(PREFIX)g++$(SUFFIX)

MAKEFLAGS += --no-builtin-rules

SRC-DIRS := src

INCLUDES := include libusb/libusb

.PHONY: all

.DEFAULT:

all : $(TARGET-DIR)/libusbuart.so

Makefile :: ;

%.mk :: ;

OBJS :=																		\
  capi.o 																	\
  ch34x.o																	\
  core.o																	\
  ftdi.o																	\
  generic.o																	\
  log.o																		\
  pl2303.o																	\


CPPFLAGS += 																\
  $(addprefix -I,$(INCLUDES))												\
  $(addprefix -D,$(CXX-DEFS))												\
  $(if $(V),-v,)															\
  -Wall																		\
  -Wextra																	\
  -O3																		\
  -fmessage-length=0														\
  -ffunction-sections  														\
  -fdata-sections															\
  -std=c++1y  																\


CFLAGS += 																	\
  $(addprefix -I,$(INCLUDES))												\
  $(addprefix -D,$(CXX-DEFS))												\
  $(if $(V),-v,)															\
  -Wall																		\
  -O3																		\
  -ffunction-sections 														\
  -fdata-sections 															\
  -std=gnu99 																\


LDFLAGS +=																	\
  -s -shared				 												\


vpath %.cpp $(subst $(eval) ,:,$(SRC-DIRS))
vpath %.c   $(subst $(eval) ,:,$(SRC-DIRS))

$(BUILD-DIR)/%.o: %.c | $(BUILD-DIR)
	@echo "     $(BOLD)cc$(NORM)" $(notdir $<)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD-DIR)/%.o: %.cpp | $(BUILD-DIR)
	@echo "    $(BOLD)c++$(NORM)" $(notdir $<)
	$(CXX) $(CPPFLAGS) -c -o $@ $<

.SECONDARY:

$(TARGET-DIR)/libusbuart.so: $(addprefix $(BUILD-DIR)/,$(OBJS)) | $(TARGET-DIR)
	@echo "    $(BOLD)ld$(NORM) " $(notdir $@)
	$(LD) $(LDFLAGS) -o $@ $^

$(BUILD-DIR)::
	@mkdir -p $@

$(TARGET-DIR)::
	@mkdir -p $@

clean:
	@rm -f $(BUILD-DIR)/*.o *.map $(TARGET-DIR)/*.so


