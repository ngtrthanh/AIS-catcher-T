SRC = Application/Main.cpp DBMS/PostgreSQL.cpp Ships/DB.cpp Application/Config.cpp Application/Receiver.cpp IO/IO.cpp IO/Server.cpp DSP/DSP.cpp Library/JSONAIS.cpp JSON/Parser.cpp JSON/StringBuilder.cpp Library/Keys.cpp Library/AIS.cpp IO/Network.cpp DSP/Model.cpp Library/NMEA.cpp Library/Utilities.cpp DSP/Demod.cpp Library/Message.cpp Device/UDP.cpp Device/RTLSDR.cpp Device/FileRAW.cpp Device/RTLTCP.cpp Library/TCP.cpp JSON/JSON.cpp

OBJ = Main.o Receiver.o Config.o PostgreSQL.o DB.o IO.o DSP.o AIS.o Model.o Utilities.o Network.o Demod.o RTLSDR.o Server.o Keys.o Parser.o StringBuilder.o FileRAW.o NMEA.o RTLTCP.o UDP.o TCP.o Message.o JSON.o JSONAIS.o

INCLUDE = -I. -IDBMS/ -IShips/ -ILibrary/ -IDSP/ -IApplication/ -IIO/
CC = gcc

override CFLAGS += -Ofast -std=c++11 $(INCLUDE)
override LFLAGS += -lstdc++ -lpthread -lm -o AIS-catcher

CFLAGS_RTL = -DHASRTLSDR $(shell pkg-config --cflags librtlsdr)

CFLAGS_SOXR = -DHASSOXR $(shell pkg-config --cflags soxr)
CFLAGS_SAMPLERATE = -DHASSAMPLERATE $(shell pkg-config --cflags samplerate)
CFLAGS_CURL = -DHASCURL $(shell pkg-config --cflags libcurl)

CFLAGS_PSQL  = -DHASPSQL ${shell pkg-config --cflags libpq}

LFLAGS_RTL = $(shell pkg-config --libs-only-l librtlsdr)

LFLAGS_SOXR = $(shell pkg-config --libs soxr)
LFLAGS_SAMPLERATE = $(shell pkg-config --libs samplerate)
LFLAGS_CURL =$(shell pkg-config --libs libcurl)
LFLAGS_ZLIB =$(shell pkg-config --libs zlib)
LFLAGS_PSQL =$(shell pkg-config --libs libpq)


CFLAGS_ALL =
LFLAGS_ALL =

ifneq ($(shell pkg-config --exists soxr && echo 'T'),)
    CFLAGS_ALL += $(CFLAGS_SOXR)
    LFLAGS_ALL += $(LFLAGS_SOXR)
endif

ifneq ($(shell pkg-config --exists samplerate && echo 'T'),)
    CFLAGS_ALL += $(CFLAGS_SAMPLERATE)
    LFLAGS_ALL += $(LFLAGS_SAMPLERATE)
endif

ifneq ($(shell pkg-config --exists librtlsdr && echo 'T'),)
    CFLAGS_ALL += $(CFLAGS_RTL)
    LFLAGS_ALL += $(LFLAGS_RTL)
endif

ifneq ($(shell pkg-config --exists libcurl && echo 'T'),)
    CFLAGS_ALL += $(CFLAGS_CURL)
    LFLAGS_ALL += $(LFLAGS_CURL)
endif

ifneq ($(shell pkg-config --exists zlib && echo 'T'),)
    CFLAGS_ALL += $(CFLAGS_ZLIB)
    LFLAGS_ALL += $(LFLAGS_ZLIB)
endif

ifneq ($(shell pkg-config --exists libpq && echo 'T'),)
    CFLAGS_ALL += $(CFLAGS_PSQL)
    LFLAGS_ALL += $(LFLAGS_PSQL)
endif

# Building AIS-Catcher

all: lib
	$(CC) $(OBJ) $(LFLAGS) $(LFLAGS_ALL)

rtl-only: lib-rtl
	$(CC) $(OBJ) $(LFLAGS) $(LFLAGS_RTL)

# Creating object-files
lib:
	$(CC) -c $(SRC) $(CFLAGS) $(CFLAGS_ALL)

lib-rtl:
	$(CC) -c $(SRC) $(CFLAGS) $(CFLAGS_RTL)

clean:
	rm *.o
	rm AIS-catcher

install:
	cp AIS-catcher /usr/local/bin/AIS-catcher
