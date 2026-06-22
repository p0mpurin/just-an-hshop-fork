
OBJS = main.o hstx.o hwav.o
CFLAGS = -pedantic -Wall -g -lavformat -lavcodec -lavutil -lswresample -lm
ifeq ($(RELEASE),1)
	CFLAGS += -DRELEASE
endif
DESTDIR ?= /usr/local
TARGET ?= 3hstool

.PHONY: clean all install
all: $(TARGET)
clean:
	@rm -f $(OBJS) $(TARGET)

install: $(TARGET)
	install -m 557 $(TARGET) $(DESTDIR)/bin

%.o: %.c
	$(CC) -c $(<) -o $(@) $(CFLAGS)

$(TARGET): $(OBJS)
	$(CC) $(^) -o $(TARGET) $(CFLAGS)

