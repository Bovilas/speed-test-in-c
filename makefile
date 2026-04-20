CC=gcc
LIBS= -lcurl -lcjson

TARGET=main.exe

SRC=main.c

all:
	$(CC) $(SRC) -o $(TARGET) $(LIBS)

clean:
	rm -f $(TARGET)