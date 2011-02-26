include config.mk

# Local directories
OBJ_DIR    = obj
SRC_DIR    = src
LIB_DIR    = lib
DOC_DIR 	 = doc

# Target
CASTING = $(LIB_DIR)/casting.so

# setting up the directory paths to search for dependency files
vpath %.h   $(SRC_DIR) 
vpath %.c   $(SRC_DIR) 
vpath %.o   $(OBJ_DIR)

# generating a list of the object files
#OBJS = casting.o lc_utils.o common.o reactor.o w_timer.o w_io.o  \
#		 lc_thread.o message.o lc_channel.o queue.o btree.o buffer.o
	
OBJS = casting.o lc_utils.o message.o buffer.o map.o queue.o \
		 lc_error.o lc_thread.o  lc_message.o lc_session.o lc_task.o lc_channel.o
# serializex.o			

# targets which don't actually refer to files
.PHONY : all clean makedirs

default: all

doc: makedirs
	@$(DOC) lua/**/*.lua -d $(DOC_DIR)
	
all: makedirs config.mk $(CASTING)

config.mk: clean

samples: all
	cd samples && $(MAKE)
	
test: all
	cd test && lua test.lua
	
bench: all
	cd bench && lua bench.lua
		
clean: makedirs
	@$(RM) $(OBJ_DIR)/*
	@$(RM) $(LIB_DIR)/*
#	@$(RM) $(DOC_DIR)/*
	
makedirs: 
	@$(MKDIR) $(OBJ_DIR)
	@$(MKDIR) $(LIB_DIR)
	@$(MKDIR) $(DOC_DIR)

%.o : %.c
	@echo "Compling		$@"
	@$(CC) $(CFLAGS) -o $(OBJ_DIR)/$(notdir $@) $<

$(CASTING): $(OBJS)
	@echo "Linking			$@"
	@$(LD) -o $@ $(LDFLAGS) $(addprefix $(OBJ_DIR)/,$(notdir $^)) $(LIBS)

install: all
	$(COPY) $(CASTING) $(LUA_LIBDIR)/$(notdir $(CASTING))
	$(COPY) lua/casting.lua $(LUA_SHAREDIR)/casting.lua
		
# Dependencies
casting.o: casting.c casting.h lc_utils.h

lc_utils.o: lc_utils.c lc_utils.h

lc_channel.o: lc_channel.c lc_channel.h queue.h

message.o: message.c message.h buffer.h map.h

queue.o: queue.c queue.h 

map.o: map.c map.h

lc_session.o: lc_session.h lc_session.c

lc_task.o: lc_task.c lc_task.h

lc_error.o: lc_error.c lc_error.h

lc_thread.o: lc_thread.h lc_thread.c

message.o: message.h message.c

lc_message.o: lc_message.c message.h 

lf_queue.o: lf_queue.h lf_queue.c
