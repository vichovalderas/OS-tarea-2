# Tarea 2 Sistemas operativos

Para compilar con el makefile:

```
make
```

o sin make:

```
gcc -Wall -Wextra -std=c17 -pthread -o torneo main.c
```

Ejecutar (N = jugadores, M = mesas simultaneas):

```
./torneo 2 1 test_cases.txt         # los 12 casos en orden
./torneo 2 1 test_cases.txt 6       # solo el caso 6
./torneo 20 2 test_cases.txt 5      # caso 5: 20 jugadores, 2 mesas
./torneo 12 3                       # sin archivo: partidas aleatorias
```

