GLIB_CFLAGS := $(shell pkg-config --cflags glib-2.0)
GLIB_LIBS := $(shell pkg-config --libs glib-2.0)

OMX_CFLAGS := -I$(PWD)/include
UTIL_CFLAGS := -I$(PWD)/util

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

$(lib): $(lib_objs)
$(lib): CFLAGS := $(CFLAGS) $(OMX_CFLAGS) $(UTIL_CFLAGS) $(GLIB_CFLAGS)
$(lib): LIBS := $(GLIB_LIBS)

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
