#
# Executable example code from the README.md file
#

NAME=vsd

DSTC_VERSION=0.6
DSTC_DIR=dstc-${DSTC_VERSION}

INCLUDE=${DSTC_DIR}/dstc.h vsd.h

SHARED_OBJ=vsd.o vsd_csv.o

TARGET_CLIENT=${NAME}_pub_example
TARGET_NOMACRO_CLIENT=${TARGET_CLIENT}_nomacro

CLIENT_OBJ=vsd_pub_example.o
CLIENT_SOURCE=$(CLIENT_OBJ:%.o=%.c)

CLIENT_NOMACRO_OBJ=$(CLIENT_OBJ:%.o=%_nomacro.o)
CLIENT_NOMACRO_SOURCE=$(CLIENT_NOMACRO_OBJ:%.o=%.c)

#
# Server
#
TARGET_SERVER=${NAME}_sub_example
TARGET_NOMACRO_SERVER=${TARGET_SERVER}_nomacro

SERVER_OBJ=vsd_sub_example.o
SERVER_SOURCE=$(SERVER_OBJ:%.o=%.c)

SERVER_NOMACRO_OBJ=$(SERVER_OBJ:%.o=%_nomacro.o)
SERVER_NOMACRO_SOURCE=$(SERVER_NOMACRO_OBJ:%.o=%.c)


LIBPATH=-L${DSTC_DIR} -L${DSTC_DIR}/reliable_multicast
INCPATH=-I${DSTC_DIR} -I${DSTC_DIR}/reliable_multicast
LIBDEPS=${DSTC_DIR}/libdstc.a ${DSTC_DIR}/reliable_multicast/librmc.a

CFLAGS= -g ${INCPATH} -Wall

.PHONY: all clean install nomacro uninstall depend

all: $(TARGET_SERVER) $(TARGET_CLIENT)

nomacro:  $(TARGET_NOMACRO_SERVER) $(TARGET_NOMACRO_CLIENT)

$(TARGET_SERVER): $(SERVER_OBJ) $(LIBDEPS)  $(SHARED_OBJ)
	$(CC) --static $(CFLAGS) $(LIBPATH) $^ -ldstc -lrmc -o $@


$(TARGET_CLIENT): $(CLIENT_OBJ) $(LIBDEPS) $(SHARED_OBJ)
	$(CC) --static $(CFLAGS) $(LIBPATH) $^ -ldstc -lrmc -o $@


# Recompile everything if dstc.h changes
$(SERVER_OBJ) $(CLIENT_OBJ) $(SHARED_OBJ): $(INCLUDE)

clean:
	rm -f $(TARGET_CLIENT) $(CLIENT_OBJ) $(TARGET_SERVER) $(SERVER_OBJ)  *~ \
	$(TARGET_NOMACRO_CLIENT) $(TARGET_NOMACRO_SERVER) \
	$(CLIENT_NOMACRO_SOURCE) $(SERVER_NOMACRO_SOURCE) \
	$(CLIENT_NOMACRO_OBJ) $(SERVER_NOMACRO_OBJ)  $(SHARED_OBJ)

install:
	install -d ${DESTDIR}/bin
	install -m 0755 ${TARGET_CLIENT} ${DESTDIR}/bin
	install -m 0755 ${TARGET_SERVER} ${DESTDIR}/bin

uninstall:
	rm ${DESTDIR}/bin/${TARGET_CLIENT}
	rm ${DESTDIR}/bin/${TARGET_SERVER}

#
# The client is built as a regular binary
#
$(TARGET_NOMACRO_CLIENT) : $(CLIENT_NOMACRO_OBJ) $(DSTC_LIB)
	$(CC) --static $(CFLAGS) $(LIBPATH) $^ -ldstc -lrmc -o $@

$(TARGET_NOMACRO_SERVER): $(SERVER_NOMACRO_OBJ) $(DSTC_LIB)
	$(CC) --static $(CFLAGS) $(LIBPATH) $^ -ldstc -lrmc -o $@


$(CLIENT_NOMACRO_SOURCE): ${CLIENT_SOURCE} ../../dstc.h
	cpp ${INCPATH} -E ${CLIENT_SOURCE} | clang-format | grep -v '^# [0-9]' > ${CLIENT_NOMACRO_SOURCE}

$(SERVER_NOMACRO_SOURCE): ${SERVER_SOURCE} ../../dstc.h
	cpp ${INCPATH} -E ${SERVER_SOURCE} | clang-format | grep -v '^# [0-9]' > ${SERVER_NOMACRO_SOURCE}

#
# Make sure the reliable_multicast is the latest copy of the submodule.
#
${DSTC_DIR}/README.md:
	curl -L https://github.com/PDXostc/dstc/archive/v${DSTC_VERSION}.tar.gz | tar xfz -

#
# Make sure the reliable_multicast submodule is up to date.
#
depend: ${DSTC_DIR}/README.md
	@$(MAKE) MAKEFLAGS=$(MAKEFLAGS) -C ${DSTC_DIR} depend
	@$(MAKE) MAKEFLAGS=$(MAKEFLAGS) -C ${DSTC_DIR}
