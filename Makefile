#
#
#

EXE_NAME = ssplash

CC = gcc
CFLAGS = -Wall -O2 -static -static-libgcc -g

DEFS = SPL_SUPPORT_JPEG
DEFS += SPL_GRAPHIC_MODE
DEFS += SPL_SUPPORT_GIF
DEFS += SPL_SUPPORT_PROGRESS_BAR
# DEFS += TERMINAL_DEBUG

INCS_DIR = 
LIBS_DIR =
BIN_DIR = /sbin
ETC_DIR = /etc

ifeq (SPL_SUPPORT_JPEG, $(findstring SPL_SUPPORT_JPEG, $(DEFS)))
LIBS += jpeg
endif

ifeq (SPL_SUPPORT_GIF, $(findstring SPL_SUPPORT_GIF, $(DEFS)))
LIBS += gif
endif

SOURCES = ssplash.c

INCS_LIST = $(addprefix -I, $(INCS_DIR))
LIBS_LIST = $(addprefix -L, $(LIBS_DIR))
LIB_LIST = $(addprefix -l, $(LIBS))
DEF_LIST = $(addprefix -D, $(DEFS))

all:
	$(CC) -o $(EXE_NAME) $(CFLAGS) $(INCS_LIST) $(LIBS_LIST) $(DEF_LIST) $(SOURCES) $(LIB_LIST)

install:
	install -vdm0755 $(DESTDIR)$(BIN_DIR)
	install -vm0744 $(EXE_NAME) $(BIN_DIR)/$(EXE_NAME)
	
	install -vdm0755 $(DESTDIR)$(ETC_DIR)
	install -vm0744 sample.conf $(ETC_DIR)/ssplash.conf

uninstall:
	rm -vf $(BIN_DIR)/$(EXE_NAME)
	rm -vf $(ETC_DIR)/ssplash.conf

clean:
	rm -f $(EXE_NAME) *.o