
IFXINC = -I/usr/include/azurepal
IFXINC += -I/usr/include/azurepal/unix/winapi
IFXINC += -I/usr/include/azurepal/unix/winapi/crt
IFXINC += -I/usr/include/azurepal/etw
IFXINC += -I/usr/include/azurepal/IfxMetrics

RUBYDIR = /opt/microsoft/omsagent/ruby/include/ruby-2.2.0
RUBYINC = -I$(RUBYDIR) -I$(RUBYDIR)/x86_64-linux

INCLUDES = $(IFXINC) $(RUBYINC)
CXXFLAGS = -g -std=c++11 -DUNICODE -D_DISABLE_POISON_ -Wno-literal-suffix -fPIC

LDFLAGS = -lIfxMetrics -lrtcpal

all: ifxext.so

ifxext.so: ifxext_wrap.o ifxext.o
	g++ -shared -fPIC ifxext_wrap.o ifxext.o -o ifxext.so $(LDFLAGS)

%.o: %.cxx
	g++ $(CXXFLAGS) $(INCLUDES) -c $<

ifxext_wrap.cxx: ifxext.h ifxext.i
	swig3.0 -c++ -ruby ifxext.i

clean:
	rm -f *.o *.so
