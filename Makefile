CC = g++
LDFLAGS = -lpthread
OUTPUT_BIN = seed

seed: seed.cpp client.cpp server.cpp
	$(CC) -o $(OUTPUT_BIN) seed.cpp client.cpp server.cpp $(LDFLAGS)
	cp $(OUTPUT_BIN) ./seed1
	cp $(OUTPUT_BIN) ./seed2
	cp $(OUTPUT_BIN) ./seed3

