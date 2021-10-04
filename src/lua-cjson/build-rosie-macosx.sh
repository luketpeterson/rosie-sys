#!/bin/bash
#
# The command below was assembled from the rosie-pattern-language Makefile (Friday, December 30, 2016)
#
make CC=gcc LUA_VERSION=5.3 PREFIX=../rosie-pattern-language/submodules/lua FPCONV_OBJS="g_fmt.o dtoa.o" CJSON_CFLAGS+=-fpic USE_INTERNAL_FPCONV=true CJSON_CFLAGS+=-DUSE_INTERNAL_FPCONV CJSON_CFLAGS+="-pthread -DMULTIPLE_THREADS" CJSON_LDFLAGS+=-pthread CJSON_LDFLAGS="-bundle -undefined dynamic_lookup"
