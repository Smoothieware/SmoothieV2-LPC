TARGET ?= a.out
SRC_DIRS ?= ./src

SRCS := $(shell find $(SRC_DIRS) -name *.cpp -or -name *.c -or -name *.s)
OBJS := $(addsuffix .o,$(basename $(SRCS)))
DEPS := $(OBJS:.o=.d)

INC_DIRS := $(shell find $(SRC_DIRS) -type d)
INC_DIRS += ./asio-1.16.1/include
INC_FLAGS := $(addprefix -I,$(INC_DIRS))

DEFINES = -DASIO_STANDALONE
CPPFLAGS ?= $(INC_FLAGS) $(DEFINES) -MMD -MP -Wall
CXXFLAGS = -std=c++11
LDLIBS = -lpthread
CC = g++

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) $(OBJS) -o $@ $(LOADLIBES) $(LDLIBS)

.PHONY: clean
clean:
	$(RM) $(TARGET) $(OBJS) $(DEPS)

-include $(DEPS)
