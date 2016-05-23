################################################################################
#   Copyright (c) 2014 NVidia Corporation
#
#   Permission is hereby granted, free of charge, to any person obtaining a copy
#   of this software and associated documentation files (the "Software"), to
#   deal in the Software without restriction, including without limitation the
#   rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
#   sell copies of the Software, and to permit persons to whom the Software is
#   furnished to do so, subject to the following conditions:
#
#       The above copyright notice and this permission notice shall be
#       included in all copies or substantial portions of the Software.
#
#   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
#   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
#   THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
#   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
#   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
#   DEALINGS IN THE SOFTWARE.
#
################################################################################

#
# Modifications to support combined source files, 9/2014 by 
# Golden G. Richard III (@nolaforensix)
#

PROGRAM_NAME=dump_fb
TEST_NAME=dump_fb_test
GDK?=/usr/include/nvidia/gdk/

CC = gcc
CXX = g++
CFLAGS = -O0 -g -Wall -Wno-format-zero-length -DNV_LINUX -DPROGRAM_NAME=\"$(PROGRAM_NAME)\"

CORE_OBJ = uvm.o
CORE_OBJ+=nvgetopt.o
CORE_OBJ+=nvidia-modprobe-utils.o
CORE_OBJ+=common-utils.o
CORE_OBJ+=msg.o

DUMP_FB_OBJ=$(CORE_OBJ) dump_fb.o 

TEST_OBJ=$(CORE_OBJ) dump_fb_test.o gtest/gtest-all.o

DRIVER_DIR?=../NVIDIA-Linux-x86_64-343.13

INCLUDES=.
INCLUDES+=$(GDK)
INCLUDES+=$(DRIVER_DIR)/kernel 
INCLUDES+=$(DRIVER_DIR)/kernel/uvm 
INCLUDES+=/usr/src/nvidia-343.13
INCLUDES+=/usr/src/nvidia-343.13/uvm

CFLAGS+=$(addprefix -I,$(INCLUDES))

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<
%.o: %.cpp
	$(CXX) --std=c++11 $(CFLAGS) -c -o $@ $<
%.o: %.cc
	$(CXX) --std=c++11 $(CFLAGS) -c -o $@ $<

.PHONY: all
all: $(PROGRAM_NAME) $(TEST_NAME)

$(PROGRAM_NAME): $(DUMP_FB_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ -L. -lnvidia-ml   

$(TEST_NAME) : $(TEST_OBJ)
	$(CXX) $(CFLAGS) -o $@ $^ -L. -lnvidia-ml  -lpthread -lrt

.PHONY: clean

clean:
	rm -f $(TEST_OBJ) $(DUMP_FB_OBJ)
