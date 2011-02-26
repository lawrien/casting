# Find out the platform we're compiling for
ifneq (,$(findstring Windows,$(OS)))
  PLAT= Windows
else
  PLAT:= $(shell uname -s)
endif

# Default commands and flags for compilation
CC  			 = gcc
LD				 = $(CC)
CFLAGS		 = -c $(PLAT_CFLAGS) $(PLAT_DEFINES) $(PLAT_INCS)
LDFLAGS		 = $(PLAT_LDFLAGS) 
LIBS			 = $(PLAT_LIBS)
#DEFINES		 = -DDEBUG=1 -DTRACE=1

# Lua install directories
LUA_DIR=/usr/local
LUA_INCDIR=$(LUA_DIR)/include
LUA_LIBDIR=$(LUA_DIR)/lib/lua/5.1
LUA_SHAREDIR=$(LUA_DIR)/share/lua/5.1

# Commands
MKDIR = mkdir -p
RM = rm -rf
COPY = cp
DOC = luadoc 

# Platform specific overrides
ifeq ($(PLAT),Linux)
	PLAT_INCS = -I/usr/include/libev $(INCS)
	PLAT_DEFINES = -D_XOPEN_SOURCE=600 $(DEFINES)
	PLAT_CFLAGS = -O2 -std=c99 -Wall -fPIC -pthread
	PLAT_LDFLAGS = -shared
	PLAT_LIBS = -lev -lpthread -lrt
endif

ifeq ($(PLAT),Darwin)
	INCS = -I/opt/local/include
	PLAT_CFLAGS = -g -Wall -fno-strict-aliasing $(DEFINES) $(INCS)
	LDFLAGS = -bundle -undefined dynamic_lookup -all_load
	PLAT_LIBS = -L/opt/local/lib -lev
endif
