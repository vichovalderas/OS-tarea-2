#ifndef TABLERO_H
#define TABLERO_H

#include "cosas.h"

typedef struct Casilla {
    int id;
    _Atomic int ocupante;
    pthread_mutex_t mutex;
    struct Casilla* vecinos[MAX_VECINOS];
    int num_vecinos;
} Casilla;

static void conectar_nodos(Casilla* a, Casilla* b) {
    if (a->num_vecinos < MAX_VECINOS && b->num_vecinos < MAX_VECINOS) {
        a->vecinos[a->num_vecinos++] = b;
        b->vecinos[b->num_vecinos++] = a;
    }
}

static Casilla* inicializar_tablero(void) {
    Casilla* t = calloc(TOTAL_CASILLAS, sizeof(Casilla));
    if (!t) return NULL;
    for (int i = 0; i < TOTAL_CASILLAS; i++) {
        t[i].id = i;
        atomic_init(&t[i].ocupante, LIBRE);
        t[i].num_vecinos = 0;
        pthread_mutex_init(&t[i].mutex, NULL);
    }
    for (int y = 0; y < 5; y++) {
        for (int x = 0; x < 5; x++) {
            int actual = y * 5 + x;
            if (x < 4) conectar_nodos(&t[actual], &t[actual + 1]);
            if (y < 4) conectar_nodos(&t[actual], &t[actual + 5]);
        }
    }
    int n_0_4 = 4 * 5 + 0, n_2_4 = 4 * 5 + 2, n_4_4 = 4 * 5 + 4;
    conectar_nodos(&t[TRONO_TOP], &t[n_2_4]);
    conectar_nodos(&t[TRONO_TOP], &t[TRONO_LEFT]);
    conectar_nodos(&t[TRONO_TOP], &t[TRONO_RIGHT]);
    conectar_nodos(&t[TRONO_LEFT], &t[n_0_4]);
    conectar_nodos(&t[TRONO_RIGHT], &t[n_4_4]);
    conectar_nodos(&t[TRONO_LEFT], &t[TRONO_RIGHT]);
    return t;
}

static void destruir_tablero(Casilla* t) {
    for (int i = 0; i < TOTAL_CASILLAS; i++) pthread_mutex_destroy(&t[i].mutex);
    free(t);
}

static bool es_adyacente(const Casilla* o, const Casilla* d) {
    for (int i = 0; i < o->num_vecinos; i++)
        if (o->vecinos[i]->id == d->id) return true;
    return false;
}

static int obtener_casilla_intermedia(int o, int d) {
    if (o < 0 || d < 0 || o >= TOTAL_CASILLAS || d >= TOTAL_CASILLAS) return -1;
    if (o < 25 && d < 25) {
        int xo = o % 5, yo = o / 5, xd = d % 5, yd = d / 5;
        if (yo == yd && abs(xo - xd) == 2) return (o + d) / 2;
        if (xo == xd && abs(yo - yd) == 2) return (o + d) / 2;
    }

    if ((o == 20 && d == TRONO_TOP) || (d == 20 && o == TRONO_TOP)) return TRONO_LEFT;
    if ((o == 24 && d == TRONO_TOP) || (d == 24 && o == TRONO_TOP)) return TRONO_RIGHT;
    if ((o == 22 && d == TRONO_LEFT) || (d == 22 && o == TRONO_LEFT)) return TRONO_TOP;
    return -1;
}

#endif
