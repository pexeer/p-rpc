# Makefile
# Copyright (C) 2017 pexeer@gamil.com
# Fri Jul 14 16:42:50 CST 2017

STD=-std=c++11
WARNING=-Wall -Werror
DEBUG=-g3
#OPT=-O2
BUILD_DIR ?= ./build
SRC_DIRS ?= ./src
#CC=clang
#CXX=clang++

SRCS := $(shell find $(SRC_DIRS) -name \*.cpp -or -name \*.c -or -name \*.S)
OBJS := $(SRCS:%=$(BUILD_DIR)/%.o)
DEPS := $(OBJS:.o=.d)
INC_DIRS := $(shell find $(SRC_DIRS) -type d)
INC_FLAGS := $(shell for subdir in $(INC_DIRS);do mkdir -p $(BUILD_DIR)/$${subdir}; done)
INC_FLAGS := $(addprefix -I,$(INC_DIRS))
#CPPFLAGS=-I./include -MMD -MP
CPPFLAGS=-I./include -I../base/include/
FINAL_ASFLAGS=$(ASFLAGS)
FINAL_CFLAGS=$(WARNING) $(OPT) $(DEBUG) $(CFLAGS) $(CPPFLAGS)
FINAL_CXXFLAGS=$(STD) $(WARNING) $(OPT) $(DEBUG) $(CPPFLAGS) $(CXXFLAGS) $(CFLAGS)
FINAL_LDFLAGS=$(LDFLAGS)  $(DEBUG)
FINAL_LIBS=-lm -ldl -pthread

CCCOLOR="\033[34m"
LINKCOLOR="\033[34;1m"
SRCCOLOR="\033[33m"
BINCOLOR="\033[37;1m"
MAKECOLOR="\033[32;1m"
ENDCOLOR="\033[0m"

ifndef V
QUIET_C = @printf '    %b %b\n' $(CCCOLOR)CC$(ENDCOLOR) $(SRCCOLOR)$@$(ENDCOLOR) 1>&2;
QUIET_CXX = @printf '    %b %b\n' $(CCCOLOR)CXX$(ENDCOLOR) $(SRCCOLOR)$@$(ENDCOLOR) 1>&2;
QUIET_LINK = @printf '%b %b\n' $(LINKCOLOR)LINK$(ENDCOLOR) $(BINCOLOR)$@$(ENDCOLOR) 1>&2;
QUIET_AR = @printf '%b %b\n' $(LINKCOLOR)AR$(ENDCOLOR) $(BINCOLOR)$@$(ENDCOLOR) 1>&2;
endif

P_AS=$(QUIET_C)$(CC) $(FINAL_ASFLAGS)
P_CC=$(QUIET_C)$(CC) $(FINAL_CFLAGS)
P_CXX=$(QUIET_CXX)$(CXX) $(FINAL_CXXFLAGS)
P_LINK=$(QUIET_LINK)$(CXX) -Wno-unused-command-line-argument $(FINAL_LDFLAGS) $(FINAL_LIBS)
P_AR=$(QUIET_AR)$(AR) crs

.PHONY: all clean

$(BUILD_DIR)/p-rpc.a: $(OBJS)
	$(P_AR) $@ $^

# dependency
-include $(DEPS)

# assembly
$(BUILD_DIR)/%.S.o: %.S
	$(P_AS) -c $< -o $@

# c source
$(BUILD_DIR)/%.c.o: %.c
	$(P_CC) -c $< -o $@

# c++ source
$(BUILD_DIR)/%.cpp.o: %.cpp
	$(P_CXX) -c $< -o $@

# exe binary
%.exe: $(BUILD_DIR)/%.cpp.o $(BUILD_DIR)/p-rpc.a ../base/build/p-base.a
	$(P_LINK) $^ -o $@

clean:
	$(RM) -r $(BUILD_DIR)
	$(RM) -r *.exe
