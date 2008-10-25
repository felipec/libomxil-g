GLIB_CFLAGS := $(shell pkg-config --cflags glib-2.0)
GLIB_LIBS := $(shell pkg-config --libs glib-2.0)

OMX_CFLAGS := -I$(PWD)/include

CORE_CFLAGS := -I$(PWD)/lib
UTIL_CFLAGS := -I$(PWD)/util
UTIL_LIBS := -L$(PWD)/util -lutil

CFLAGS ?= -Wall -ggdb -ansi -std=c99

all:

util := util/libutil.a
util_objs := util/async_queue.o

$(util): $(util_objs)
$(util): CFLAGS := $(CFLAGS) $(GLIB_CFLAGS)
$(util): LIBS := $(GLIB_LIBS)

targets += $(util)
objs += $(util_objs)

lib := libomxil-g.so
lib_objs := lib/core.o

# dummy
lib_objs += lib/dummy/module.o

# x264
X264_CFLAGS := $(shell pkg-config --cflags x264)
X264_LIBS := $(shell pkg-config --libs x264)

lib_objs += lib/x264/module.o
lib_cflags += $(X264_CFLAGS)
lib_libs += $(X264_LIBS)

$(lib): $(lib_objs)
$(lib): CFLAGS := $(CFLAGS) $(OMX_CFLAGS) $(CORE_CFLAGS) $(UTIL_CFLAGS) $(GLIB_CFLAGS) $(lib_cflags)
$(lib): LIBS := $(UTIL_LIBS) $(GLIB_LIBS) $(lib_libs)

targets += $(lib)
objs += $(lib_objs)

deps += $(objs:.o=.d)

all: $(targets)

# from Lauri Leukkunen's build system
ifdef V
Q = 
P = @printf "" # <- space before hash is important!!!
else
P = @printf "[%s] $@\n" # <- space before hash is important!!!
Q = @
endif

%.a::
	$(P)AR
	$(Q)$(AR) rcs $@ $^

%.o:: %.c
	$(P)CC
	$(Q)$(CC) $(CFLAGS) -MMD -o $@ -c $<

%.so::
	$(P)SHLIB
	$(Q)$(CC) $(LDFLAGS) -shared -o $@ $^ $(LIBS)

clean:
	$(Q)$(RM) $(targets) $(objs) $(deps)

-include $(deps)
