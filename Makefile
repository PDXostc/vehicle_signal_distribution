#
# Makefile for the Vehicle Signal Distribution system.
#

NAME=vsd

DESTDIR ?= /usr/local

INCLUDE=vehicle_signal_distribution.h vsd_internal.h uthash.h

SHARED_OBJ=vsd.o vsd_csv.o

TARGET_SO=libvsd.so

CFLAGS?= -g -Wall -I/usr/local -fPIC

.PHONY: all clean install nomacro uninstall examples install_examples

all: $(TARGET_SO)

nomacro:
	$(MAKE) -C examples nomacro

$(TARGET_SO): $(SHARED_OBJ)
	$(CC) --shared $(CFLAGS) $^ $(LDFLAGS) -o $@

# Recompile everything if dstc.h changes
$(SHARED_OBJ): $(INCLUDE)

.c.o:
	$(CC) -c $(CFLAGS) $^

clean:
	rm -f   *~ $(SHARED_OBJ) $(TARGET_SO)
	$(MAKE) -C examples clean

install:
	install -d ${DESTDIR}/lib
	install -d ${DESTDIR}/include
	install -d ${DESTDIR}/share
	install -m 0644 ${TARGET_SO} ${DESTDIR}/lib
	install -m 0644 ${INCLUDE} ${DESTDIR}/include
	install -m 0644 vss_rel_*.csv ${DESTDIR}/share

uninstall:
	rm -f ${DESTDIR}/share/vss_rel_*.csv
	rm -f ${DESTDIR}/lib/${TARGET_SO}
	rm -f ${DESTDIR}/include/${INCLUDE}

examples:
	$(MAKE) -C examples

install_examples:
	$(MAKE) -C examples install
