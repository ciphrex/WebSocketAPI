CXXFLAGS += -Wall -O3

ifndef OS
    UNAME_S := $(shell uname -s)
    ifeq ($(UNAME_S), Linux)
        OS = linux
    else ifeq ($(UNAME_S), Darwin)
        OS = osx
    endif
endif

ifeq ($(OS), linux)
    CXX = g++
    CC = gcc
    CXXFLAGS += -Wno-unknown-pragmas -std=c++0x -DBOOST_SYSTEM_NOEXCEPT=""

    ARCHIVER = ar

else ifeq ($(OS), mingw64)
    CXX =  x86_64-w64-mingw32-g++
    CC =  x86_64-w64-mingw32-gcc
    CXXFLAGS += -Wno-unknown-pragmas -Wno-strict-aliasing -std=c++0x -DBOOST_SYSTEM_NOEXCEPT=""

    MINGW64_ROOT = /usr/x86_64-w64-mingw32

    INCLUDE_PATH += -I$(MINGW64_ROOT)/include

    ARCHIVER = x86_64-w64-mingw32-ar

    EXE_EXT = .exe

else ifeq ($(OS), osx)
    CXX = clang++
    CC = clang
    CXXFLAGS += -Wno-unknown-pragmas -Wno-unneeded-internal-declaration -std=c++11 -stdlib=libc++ -DBOOST_THREAD_DONT_USE_CHRONO -DMAC_OS_X_VERSION_MIN_REQUIRED=MAC_OS_X_VERSION_10_6 -mmacosx-version-min=10.7

    INCLUDE_PATH += -I/usr/local/include

    ARCHIVER = ar

    BOOST_SUFFIX = -mt

else ifneq ($(MAKECMDGOALS), clean)
    $(error OS must be set to linux, mingw64, or osx)
endif

INCLUDE_PATH = \
    -Isrc

LIB_PATH = \
    -Llib

OBJS = \
    obj/JsonRpc.o \
    obj/WebSocketServer.o

LIBS = \
    -lboost_system$(BOOST_SUFFIX) \
    -lboost_regex$(BOOST_SUFFIX) \
    -lboost_thread$(BOOST_SUFFIX) \
    -lWebSocketServer

all: lib tests

lib: lib/libWebSocketServer.a

lib/libWebSocketServer.a: $(OBJS)
	$(ARCHIVER) rcs $@ $^

obj/%.o: src/%.cpp src/%.h
	$(CXX) $(CXXFLAGS) $(INCLUDE_PATH) -c $< -o $@

tests: tests/build/WebSocketServerTest

tests/build/WebSocketServerTest: tests/src/WebSocketServerTest.cpp lib/libWebSocketServer.a
	$(CXX) $(CXXFLAGS) $(INCLUDE_PATH) $(LIB_PATH) $(LIBS) $< -o $@

clean:
	-rm -f obj/*.o lib/*.a tests/build/WebSocketServerTest
