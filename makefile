CC = gcc

CFLAGS = -Wall -Wextra -std=c11 -pthread
LIBS = -lcurl -lcjson

TARGET = speedtest

SOURCES = \
	main.c \
	common.c \
	location.c \
	servers.c \
	speedtest.c

OBJECTS = $(SOURCES:.c=.o)
DEPENDS = $(OBJECTS:.o=.d)


$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $@ $(LIBS)


%.o: %.c
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@


-include $(DEPENDS)


clean:
	rm -f $(OBJECTS) $(DEPENDS) $(TARGET)


run: $(TARGET)
	./$(TARGET)