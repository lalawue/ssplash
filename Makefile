CC = gcc
CFLAGS = -Wall -O2 -static -static-libgcc -g

DEFS = SPL_SUPPORT_JPEG
DEFS += SPL_GRAPHIC_MODE
DEFS += SPL_SUPPORT_GIF
DEFS += SPL_SUPPORT_PROGRESS_BAR
# DEFS += TERMINAL_DEBUG

INCS_DIR = 
LIBS_DIR =
DESTDIR =

ifeq (SPL_SUPPORT_JPEG, $(findstring SPL_SUPPORT_JPEG, $(DEFS)))
LIBS += jpeg
endif

ifeq (SPL_SUPPORT_GIF, $(findstring SPL_SUPPORT_GIF, $(DEFS)))
LIBS += gif
endif

INCS_LIST = $(addprefix -I, $(INCS_DIR))
LIBS_LIST = $(addprefix -L, $(LIBS_DIR))
LIB_LIST = $(addprefix -l, $(LIBS))
DEF_LIST = $(addprefix -D, $(DEFS))

all:
	$(CC) -o splash $(CFLAGS) $(INCS_LIST) $(LIBS_LIST) $(DEF_LIST) src/splash.c $(LIB_LIST)

install:
	install -vdm0755 $(DESTDIR)/sbin
	install -vm0744 splash $(DESTDIR)/sbin/splash
	
	install -vdm0755 $(DESTDIR)/etc
	install -vm0644 etc/splash.conf $(DESTDIR)/etc/splash.conf
	
	install -vdm0755 $(DESTDIR)/lib/splash
	install -vm0644 lib/splash.jpg $(DESTDIR)/lib/splash/splash.jpg
	
	install -vdm0755 $(DESTDIR)/usr/lib/initcpio/{hooks,install}
	install -vm0644 initcpio/hook $(DESTDIR)/usr/lib/initcpio/hooks/splash
	install -vm0644 initcpio/install $(DESTDIR)/usr/lib/initcpio/install/splash

uninstall:
	rm -vf $(DESTDIR)/sbin/splash
	rm -vf $(DESTDIR)/etc/splash.conf
	rm -vf $(DESTDIR)/lib/splash/splash.jpg

clean:
	rm -f splash *.o