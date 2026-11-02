CC      = gcc
CFLAGS  = -Wall -Wextra -std=c17 -pthread -O2
HDRS    = cosas.h util.h estado.h tablero.h config.h mesa.h juez.h matchmaker.h
TARGET  = torneo

# Todo el programa es una sola unidad de compilacion: main.c incluye los modulos (.h)
$(TARGET): main.c $(HDRS)
	$(CC) $(CFLAGS) -o $@ main.c

# Corre los 12 casos de test_cases.txt (2 jugadores, 1 mesa por caso)
test: $(TARGET)
	./$(TARGET) 2 1 test_cases.txt

# Version con ThreadSanitizer para detectar data races
tsan: main.c $(HDRS)
	$(CC) -Wall -Wextra -std=c17 -pthread -g -O1 -fsanitize=thread -o $(TARGET)_tsan main.c

clean:
	rm -f $(TARGET) $(TARGET)_tsan

.PHONY: test tsan clean
