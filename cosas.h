#ifndef COSAS
#define COSAS

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define TOTAL_CASILLAS 28
#define MAX_VECINOS 5
#define TRONO_TOP 25
#define TRONO_LEFT 26
#define TRONO_RIGHT 27
#define NUM_FICHAS 13
#define NUM_PERROS 12
#define MAX_INSTR 64
#define MAX_CASOS 64
#define CAPTURAS_VICTORIA 6
#define LIBRE (-1)
#define POLL_JUEZ_MS 50
#define SIGINT_SIMULADO_MS 250

#endif
