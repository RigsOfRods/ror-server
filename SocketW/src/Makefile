# Makefile for the C++ socket wrapper library

include ../Makefile.conf

CFLAGS += $(SW_CFLAGS) -fPIC -I./
LIBS = $(SW_LIBS)

SW_VER = 031026
API_VER = 0

TARGETS = sw_base sw_unix sw_inet sw_ssl
OBJECTS = $(addsuffix .o, $(TARGETS))


all:	$(OBJECTS)
	@ar rsc libSocketW.a $(OBJECTS)
	
$(OBJECTS):	%.o:%.cxx %.h   #Each object depends on thier .cxx and .h file
	$(CXX) $(CFLAGS) -c $<

shared: all
	$(CXX) -Wl,-soname,libSocketW.so.$(API_VER) -fpic -fPIC -shared -o libSocketW.so.$(API_VER).$(SW_VER) $(OBJECTS) $(LIBS)
	@ln -sf libSocketW.so.$(API_VER).$(SW_VER) libSocketW.so.$(API_VER)
	@ln -sf libSocketW.so.$(API_VER) libSocketW.so

shared-strip:	shared
	@strip libSocketW.so.$(API_VER).$(SW_VER)

dll:	$(OBJECTS)
	dlltool --output-def SocketW.def $(OBJECTS)
	dllwrap --driver-name $(CXX) -o SocketW.dll --def SocketW.def --output-lib libSocketW.a --dllname SocketW.dll $(OBJECTS) $(LIBS)

dll-strip:	dll
	@strip SocketW.dll

install:	shared
	@mkdir -p $(PREFIX_H)
	install -c -m 644 *.h $(PREFIX_H)
	@mkdir -p $(PREFIX)/lib
	install -c -m 644 libSocketW.a $(PREFIX)/lib
	install -c libSocketW.so.$(API_VER).$(SW_VER) $(PREFIX)/lib
	@cd $(PREFIX)/lib;\
	ln -sf libSocketW.so.$(API_VER).$(SW_VER) libSocketW.so.$(API_VER);\
	ln -sf libSocketW.so.$(API_VER) libSocketW.so
	@echo "** Headerfiles installed in $(PREFIX_H)"
	@echo "** Library files installed in $(PREFIX)/lib"

clean:
	@rm -f *.o *.so *.so.* *.a *.dll *.def
