CC=g++
CFLAGS=-c -ISocketW/src
LDFLAGS= SocketW/src/*.o -lpthread
SOURCES=broadcaster.cpp listener.cpp messaging.cpp notifier.cpp receiver.cpp rorserver.cpp sequencer.cpp
OBJECTS=$(SOURCES:.cpp=.o)
EXECUTABLE=rorserver

all: $(SOURCES) $(EXECUTABLE)
	
socketw:
	@(cd SocketW; make)

$(EXECUTABLE): $(OBJECTS) socketw
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@

.cpp.o:
	$(CC) $(CFLAGS) $< -o $@


clean:
	rm -f $(OBJECTS) $(EXECUTABLE) SocketW/src/*.o SocketW/src/libSocketW.a
