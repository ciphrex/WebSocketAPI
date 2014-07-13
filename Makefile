CXXFLAGS += -Wall -O3

ifndef OS
    UNAME_S := $(shell uname -s)
    ifeq ($(UNAME_S), Linux)
        OS = linux
    else ifeq ($(UNAME_S), Darwin)
        OS = osx
    endif
endif

ifndef SYSROOT
    SYSROOT = /usr/local
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

    BOOST_THREAD_SUFFIX = _win32
    BOOST_SUFFIX = -mt-s

    PLATFORM_LIBS += \
        -static-libgcc -static-libstdc++ \
        -lws2_32 \
        -lmswsock

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

INCLUDE_PATH += \
    -Isrc

LIB_PATH += \
    -Llib

# libboost_regex is needed by server
# libboost_random is needed by client
LIBS += \
    -lWebSocketServer \
    -lWebSocketClient \
    -lJsonRpc \
    -lboost_system$(BOOST_SUFFIX) \
    -lboost_regex$(BOOST_SUFFIX) \
    -lboost_random$(BOOST_SUFFIX) \
    -lboost_thread$(BOOST_THREAD_SUFFIX)$(BOOST_SUFFIX)

all: libs tests

libs: jsonrpc server client

# JSON-RPC
jsonrpc: lib/libJsonRpc.a

lib/libJsonRpc.a: obj/JsonRpc.o
	$(ARCHIVER) rcs $@ $^

obj/JsonRpc.o: src/JsonRpc.cpp src/JsonRpc.h
	$(CXX) $(CXXFLAGS) $(INCLUDE_PATH) -c $< -o $@

# Server
server: jsonrpc lib/libWebSocketServer.a

lib/libWebSocketServer.a: obj/WebSocketServer.o obj/WebSocketServerTls.o
	$(ARCHIVER) rcs $@ $^

obj/WebSocketServer.o: src/WebSocketServer.cpp src/WebSocketServer.h
	$(CXX) $(CXXFLAGS) $(INCLUDE_PATH) -c $< -o $@

obj/WebSocketServerTls.o: src/WebSocketServer.cpp src/WebSocketServer.h
	$(CXX) -DUSE_TLS $(CXXFLAGS) $(INCLUDE_PATH) -c $< -o $@

# Client
client: jsonrpc lib/libWebSocketClient.a

lib/libWebSocketClient.a: obj/WebSocketClient.o
	$(ARCHIVER) rcs $@ $^

obj/WebSocketClient.o: src/WebSocketClient.cpp src/WebSocketClient.h
	$(CXX) $(CXXFLAGS) $(INCLUDE_PATH) -c $< -o $@

tests: server_tests client_tests

# Server Tests
server_tests: tests/build/WebSocketServerTest$(EXE_EXT) tests/build/WebSocketServerTlsTest$(EXE_EXT)

tests/build/WebSocketServerTest$(EXE_EXT): tests/src/WebSocketServerTest.cpp lib/libWebSocketServer.a lib/libJsonRpc.a
	$(CXX) $(CXXFLAGS) $(INCLUDE_PATH) $(LIB_PATH) $< -o $@ $(LIBS) $(PLATFORM_LIBS)

tests/build/WebSocketServerTlsTest$(EXE_EXT): tests/src/WebSocketServerTlsTest.cpp lib/libWebSocketServer.a lib/libJsonRpc.a
	$(CXX) $(CXXFLAGS) $(INCLUDE_PATH) $(LIB_PATH) $< -o $@ -lcrypto -lssl $(LIBS) $(PLATFORM_LIBS)

# Client Tests
client_tests: tests/build/WebSocketClientTest$(EXE_EXT)

tests/build/WebSocketClientTest$(EXE_EXT): tests/src/WebSocketClientTest.cpp lib/libWebSocketClient.a
	$(CXX) $(CXXFLAGS) $(INCLUDE_PATH) $(LIB_PATH) $< -o $@ $(LIBS) $(PLATFORM_LIBS)

install: install_jsonrpc install_server install_client

install_jsonrpc:
	-mkdir -p $(SYSROOT)/include/WebSocketAPI
	-rsync -u src/JsonRpc.h $(SYSROOT)/include/WebSocketAPI/
	-mkdir -p $(SYSROOT)/lib
	-rsync -u lib/libJsonRpc.a $(SYSROOT)/lib/

install_server: install_jsonrpc
	-rsync -u src/WebSocketServer.h $(SYSROOT)/include/WebSocketAPI/
	-rsync -u lib/libWebSocketServer.a $(SYSROOT)/lib/

install_client: install_jsonrpc
	-rsync -u src/WebSocketClient.h $(SYSROOT)/include/WebSocketAPI/
	-rsync -u lib/libWebSocketClient.a $(SYSROOT)/lib/

remove:
	-rm -rf $(SYSROOT)/include/WebSocketAPI
	-rm $(SYSROOT)/lib/libJsonRpc.a
	-rm $(SYSROOT)/lib/libWebSocketServer.a
	-rm $(SYSROOT)/lib/libWebSocketClient.a
	
clean:
	-rm -f obj/*.o lib/*.a tests/build/WebSocketServerTest tests/build/WebSocketServerTlsTest
