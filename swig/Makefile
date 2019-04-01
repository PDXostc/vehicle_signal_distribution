#
# Makefile to generate python interface code for DSTC
#

CFLAGS += -ggdb

vsd_swig_wrap.o: vsd_swig_wrap.c
	python3 setup.py build_ext --inplace

vsd_swig_wrap.c: vsd_swig.i ../vehicle_signal_distribution.h
	swig -I/usr/include -python -includeall vsd_swig.i

clean:
	rm -rf _vsd*.so build vsd_swig.py vsd_swig_wrap.* __pycache__
