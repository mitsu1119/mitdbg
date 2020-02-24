DST = ./dst
INCLUDE = ./include
SRC = ./src
OBJS = $(DST)/main.o $(DST)/mitdbg.o
CFLAGS = -c -Wall -std=c++17

.PHONY: all
all: mitdbg

mitdbg: $(INCLUDE) $(OBJS) $(DST)
	g++ $(OBJS) -o ./mitdbg

$(DST)/%.o: $(SRC)/%.cpp $(DST)
	g++ $(CFLAGS) $(SRC)/$*.cpp -I $(INCLUDE) -o $(DST)/$*.o

$(DST):
	mkdir -p $(DST)

clean:
	rm -rf $(DST)
