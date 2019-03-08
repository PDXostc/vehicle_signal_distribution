#
# Makefile for the Vehicle Signal Distribution system.
#

NAME=vsd

DSTC_VERSION=0.6
DSTC_DIR=dstc-${DSTC_VERSION}

DESTDIR ?= /usr/local

INCLUDE=vsd.h

SHARED_OBJ=vsd.o vsd_csv.o

TARGET_OBJECT=libvsd.so

EXAMPLE_TARGET_CLIENT=${NAME}_pub_example
TARGET_NOMACRO_CLIENT=${EXAMPLE_TARGET_CLIENT}_nomacro

CLIENT_OBJ=vsd_pub_example.o
CLIENT_SOURCE=$(CLIENT_OBJ:%.o=%.c)

CLIENT_NOMACRO_OBJ=$(CLIENT_OBJ:%.o=%_nomacro.o)
CLIENT_NOMACRO_SOURCE=$(CLIENT_NOMACRO_OBJ:%.o=%.c)

#
# Server
#
EXAMPLE_TARGET_SERVER=${NAME}_sub_example
TARGET_NOMACRO_SERVER=${EXAMPLE_TARGET_SERVER}_nomacro

SERVER_OBJ=vsd_sub_example.o
SERVER_SOURCE=$(SERVER_OBJ:%.o=%.c)

SERVER_NOMACRO_OBJ=$(SERVER_OBJ:%.o=%_nomacro.o)
SERVER_NOMACRO_SOURCE=$(SERVER_NOMACRO_OBJ:%.o=%.c)

CFLAGS= -g -Wall -I/usr/local -fPIC
LFLAGS= -L/usr/lib -ldstc -lrmc

.PHONY: all clean install nomacro uninstall

all: $(EXAMPLE_TARGET_SERVER) $(EXAMPLE_TARGET_CLIENT) $(TARGET_OBJECT)

nomacro:  $(TARGET_NOMACRO_SERVER) $(TARGET_NOMACRO_CLIENT)

$(TARGET_OBJECT): $(SHARED_OBJ)
	$(CC) --shared $(CFLAGS) $^ $(LFLAGS) -o $@

$(EXAMPLE_TARGET_SERVER): $(SERVER_OBJ) $(SHARED_OBJ)
	$(CC) $(CFLAGS) $^ $(LFLAGS) -o $@


$(EXAMPLE_TARGET_CLIENT): $(CLIENT_OBJ) $(SHARED_OBJ)
	$(CC) $(CFLAGS) $^ $(LFLAGS) -o $@


# Recompile everything if dstc.h changes
$(SERVER_OBJ) $(CLIENT_OBJ) $(SHARED_OBJ): $(INCLUDE)

clean:
	rm -f $(EXAMPLE_TARGET_CLIENT) $(CLIENT_OBJ) $(EXAMPLE_TARGET_SERVER) $(SERVER_OBJ)  *~ \
	$(TARGET_NOMACRO_CLIENT) $(TARGET_NOMACRO_SERVER) \
	$(CLIENT_NOMACRO_SOURCE) $(SERVER_NOMACRO_SOURCE) \
	$(CLIENT_NOMACRO_OBJ) $(SERVER_NOMACRO_OBJ)  $(SHARED_OBJ) $(TARGET_OBJECT)

install:
	install -d ${DESTDIR}/bin
	install -m 0755 ${EXAMPLE_TARGET_CLIENT} ${DESTDIR}/bin
	install -m 0755 ${EXAMPLE_TARGET_SERVER} ${DESTDIR}/bin
	install -m 0755 vss_rel_2.0.0-alpha+005.csv ${DESTDIR}/share/vss_rel_2.0.0-alpha+005.csv

uninstall:
	rm -f ${DESTDIR}/bin/${EXAMPLE_TARGET_CLIENT}
	rm -f ${DESTDIR}/bin/${EXAMPLE_TARGET_SERVER}
	rm -f ${DESTDIR}/share/vss_rel_2.0.0-alpha+005.csv
#
# The client is built as a regular binary
#
$(TARGET_NOMACRO_CLIENT) : $(CLIENT_NOMACRO_OBJ)
	$(CC) --static $(CFLAGS) $^ -ldstc -lrmc -o $@

$(TARGET_NOMACRO_SERVER): $(SERVER_NOMACRO_OBJ)
	$(CC) --static $(CFLAGS) $^ -ldstc -lrmc -o $@


$(CLIENT_NOMACRO_SOURCE): ${CLIENT_SOURCE} -Idstc.h
	cpp ${INCPATH} -E ${CLIENT_SOURCE} | clang-format | grep -v '^# [0-9]' > ${CLIENT_NOMACRO_SOURCE}

$(SERVER_NOMACRO_SOURCE): ${SERVER_SOURCE} -Idstc.h
	cpp ${INCPATH} -E ${SERVER_SOURCE} | clang-format | grep -v '^# [0-9]' > ${SERVER_NOMACRO_SOURCE}
