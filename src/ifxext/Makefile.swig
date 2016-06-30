all: ifxext_wrap.cxx

ifxext_wrap.cxx: ifxext.h ifxext.i
	swig3.0 -c++ -ruby ifxext.i
