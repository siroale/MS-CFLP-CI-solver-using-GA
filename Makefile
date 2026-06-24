CXX = g++
CXXFLAGS = -std=c++17 -O3 -Wall

TARGET = cflp_ci
SRC = cflp_ci.cpp

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET) *.o solucion_*.txt convergencia_*.csv
