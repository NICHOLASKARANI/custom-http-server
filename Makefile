CC = gcc
CFLAGS = -Wall -Wextra -O2 -pthread
LDFLAGS = -pthread

TARGET = http-server
SRCS = src/main.c src/server.c src/request.c src/response.c src/routing.c src/proxy.c
OBJS = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

run: $(TARGET)
	./$(TARGET)

install:
	mkdir -p logs www
	cp config/server.conf .
	cp -r www/ .

.PHONY: all clean run install