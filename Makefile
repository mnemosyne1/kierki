CXX     = g++
CXXFLAGS = -Wall -Wextra -O2 -std=c++20

.PHONY: all clean

TARGET1 = kierki-klient
TARGET2 = kierki-serwer

all: $(TARGET1) $(TARGET2)

$(TARGET1): $(TARGET1).o err.o card.o
	$(CXX) $(CXXFLAGS) -o $@ $^
$(TARGET2): $(TARGET2).o err.o card.o
	$(CXX) $(CXXFLAGS) -o $@ $^


card.o: card.cpp card.h
	$(CXX) $(CXXFLAGS) -c $< -o $@
kierki-klient.o: client.cpp client_parser.h
	$(CXX) $(CXXFLAGS) -c $< -o $@
kierki-serwer.o: server.cpp server_parser.h
	$(CXX) $(CXXFLAGS) -c $< -o $@
err.o: err.c err.h


clean:
	rm -f $(TARGET1) $(TARGET2) *.o *~
