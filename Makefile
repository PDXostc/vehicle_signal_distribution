#
# Makefile for the Vehicle Signal Distribution system.
#

NAME=vsd

DESTDIR ?= /usr/local

INCLUDE=vehicle_signal_distribution.h

SHARED_OBJ=vsd.o
TARGET_SO=libvsd.so

CFLAGSLIST= -ggdb -Wall -I/usr/local -fPIC $(CFLAGS) $(CPPFLAGS)


.PHONY: all clean install nomacro uninstall examples install_examples

export CFLAGSLIST

all: $(TARGET_SO)

nomacro:
	$(MAKE) -C examples nomacro

$(TARGET_SO): $(SHARED_OBJ)
	$(LD) --shared $^ $(LDFLAGS) -o $@

# Recompile everything if dstc.h changes
$(SHARED_OBJ): $(INCLUDE)

.c.o:
	$(CC) -c $(CFLAGS) $<

clean:
	rm -f   *~ $(SHARED_OBJ) $(TARGET_SO)
	$(MAKE) -C examples clean

install:
	install -d ${DESTDIR}/lib
	install -d ${DESTDIR}/include
	install -d ${DESTDIR}/share
	install -m 0644 ${TARGET_SO} ${DESTDIR}/lib
	install -m 0644 ${INCLUDE} ${DESTDIR}/include

uninstall:
	rm -f ${DESTDIR}/lib/${TARGET_SO}
	(cd ${DESTDIR}/include; rm -f ${INCLUDE})

examples:
	$(MAKE) -C examples

install_examples:
	$(MAKE) -C examples install
