# Makefile for the C++ socket wrapper library examples

include ../Makefile.conf

CFLAGS += $(SW_CFLAGS) -I./../src/
LIBS =-L./../src/ -lSocketW $(SW_LIBS)

TARGETS = client server ssl_telnet
OBJECTS = $(addsuffix .o, $(TARGETS))


all:	$(TARGETS)

$(TARGETS):	%:%.o
	$(CXX) -o $@ $< $(LIBS)		

$(OBJECTS):	%.o:%.cxx
	$(CXX) $(CFLAGS) -c $< 	

strip:	all
	@strip $(TARGETS)

clean:
	@rm -f *.o $(TARGETS)
