CXX = clang++
CXXFLAGS += -Wall -O3

ARCHIVER = ar

INCLUDE_PATH = \
    -Isrc

LIB_PATH = \
    -Llib

OBJS = \
    obj/JsonRpc.o \
    obj/WebSocketServer.o

BOOST_SUFFIX = -mt

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
