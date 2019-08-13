NAME=vsd

DESTDIR ?= /usr/local
INCLUDE=../vehicle_signal_distribution.h
SHARED_OBJ=../vsd.o

VSS_HDR=vss.h vss_macro.h
VSS_SPEC_PATH ?= /usr/local/share/vss/
VSS_TOOLS_PATH ?= /usr/local/bin


#
# Client
#

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

LFLAGS= -L/usr/local/lib -lvss -ldstc -lrmc
CFLAGS?= -ggdb -Wall -I../ -I/usr/local

.PHONY: all clean install

all: ${VSS_HDR} $(EXAMPLE_TARGET_SERVER) $(EXAMPLE_TARGET_CLIENT)

nomacro: ${VSS_HDR} $(TARGET_NOMACRO_SERVER) $(TARGET_NOMACRO_CLIENT)

$(EXAMPLE_TARGET_SERVER): $(SERVER_OBJ) $(SHARED_OBJ)
	$(CC) $(CFLAGS) $^ $(LFLAGS) -o $@ $(LDFLAGS)


$(EXAMPLE_TARGET_CLIENT): $(CLIENT_OBJ) $(SHARED_OBJ)
	$(CC) $(CFLAGS) $^ $(LFLAGS) -o $@ $(LDFLAGS)

#
# Recompile server and client if headers change
#
$(CLIENT_OBJ) $(SHARED_OBJ): $(INCLUDE)

clean:
	rm -f $(EXAMPLE_TARGET_CLIENT) $(EXAMPLE_TARGET_SERVER) $(SERVER_OBJ) $(CLIENT_OBJ) \
	$(CLIENT_NOMACRO_OBJ) $(SERVER_NOMACRO_OBJ) $(TARGET_NOMACRO_CLIENT) \
	$(TARGET_NOMACRO_SERVER) $(CLIENT_NOMACRO_SOURCE) \
	$(SERVER_NOMACRO_SOURCE) ${VSS_HDR}

install:
	install -d ${DESTDIR}/bin
	install -m 0755 ${EXAMPLE_TARGET_CLIENT} ${DESTDIR}/bin
	install -m 0755 ${EXAMPLE_TARGET_SERVER} ${DESTDIR}/bin

#
# The client is built as a regular binary
#
$(TARGET_NOMACRO_CLIENT) : $(CLIENT_NOMACRO_OBJ)
	$(CC) $(CFLAGS) $^ -ldstc -lrmc -lvsd -o $@ $(LDFLAGS)

$(TARGET_NOMACRO_SERVER): $(SERVER_NOMACRO_OBJ)
	$(CC) $(CFLAGS) $^ -ldstc -lrmc -lvsd -o $@ $(LDFLAGS)

$(CLIENT_NOMACRO_SOURCE): ${CLIENT_SOURCE} /usr/local/include/dstc.h
	cpp -E ${CLIENT_SOURCE} | clang-format | grep -v '^# [0-9]' > ${CLIENT_NOMACRO_SOURCE}

$(SERVER_NOMACRO_SOURCE): ${SERVER_SOURCE} /usr/local/include/dstc.h
	cpp -E ${SERVER_SOURCE} | clang-format | grep -v '^# [0-9]' > ${SERVER_NOMACRO_SOURCE}

${VSS_HDR}:
	${VSS_TOOLS_PATH}/vspec2c.py -I${VSS_SPEC_PATH} -i:${VSS_SPEC_PATH}/VehicleSignalSpecification.id ${VSS_SPEC_PATH}/VehicleSignalSpecification.vspec ${VSS_HDR}
