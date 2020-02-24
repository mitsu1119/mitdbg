DST = ./dst
INCLUDE = ./include
SRC = ./src
OBJS = $(DST)/main.o

.PHONY: all
all: mitdbg

mitdbg: $(INCLUDE) $(OBJS) $(DST)
	g++ $(OBJS) -o ./mitdbg

$(DST)/%.o: $(SRC)/%.cpp $(DST)
	g++ -c $(SRC)/$*.cpp -I $(INCLUDE) -o $(DST)/$*.o

$(DST):
	mkdir -p $(DST)

clean:
	rm -rf $(DST)
