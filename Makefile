CC = gcc
CFLAGS = -Wall -O2 -static -static-libgcc -g

# -DTERMINAL_DEBUG  

all:
	$(CC) -o splash -DSPL_GRAPHIC_MODE src/splash.c $(LIB_LIST)

install:
	install -vdm0755 $(DESTDIR)/sbin
	install -vm0744 splash $(DESTDIR)/sbin/splash
	
	install -vdm0755 $(DESTDIR)/etc
	install -vm0644 etc/splash.conf $(DESTDIR)/etc/splash.conf
	
	install -vdm0755 $(DESTDIR)/usr/lib/initcpio/{hooks,install}
	install -vm0644 initcpio/hook $(DESTDIR)/usr/lib/initcpio/hooks/splash
	install -vm0644 initcpio/install $(DESTDIR)/usr/lib/initcpio/install/splash

uninstall:
	rm -vf $(DESTDIR)/sbin/splash
	rm -vf $(DESTDIR)/etc/splash.conf
	rm -vf $(DESTDIR)/usr/lib/initcpio/{hooks,install}/splash

clean:
	rm -f splash *.o
