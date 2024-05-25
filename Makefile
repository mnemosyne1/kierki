CXX     = g++
CXXFLAGS = -Wall -Wextra -O2 -std=c++2b

.PHONY: all clean

TARGET1 = kierki-klient
TARGET2 = kierki-serwer

all: $(TARGET1) $(TARGET2)

$(TARGET1): $(TARGET1).o err.o card.o common.o
	$(CXX) $(CXXFLAGS) -o $@ $^
$(TARGET2): $(TARGET2).o err.o card.o common.o server_players.o
	$(CXX) $(CXXFLAGS) -o $@ $^

kierki-klient.o: client.cpp parser.h client_communication.h common.h
	$(CXX) $(CXXFLAGS) -c $< -o $@
kierki-serwer.o: server.cpp parser.h server_communication.h server_players.h
	$(CXX) $(CXXFLAGS) -c $< -o $@
common.o: common.cpp common.h card.h
	$(CXX) $(CXXFLAGS) -c $< -o $@
server_players.o: server_players.cpp server_players.h common.h err.h card.h server_inside.h
	$(CXX) $(CXXFLAGS) -c $< -o $@
%.o: %.cpp %.h
	$(CXX) $(CXXFLAGS) -c $< -o $@


clean:
	rm -f $(TARGET1) $(TARGET2) *.o *~
