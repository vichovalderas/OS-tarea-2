#ifndef JUEZ_H
#define JUEZ_H

#include "cosas.h"
#include "config.h"
#include "estado.h"
#include "mesa.h"
#include "tablero.h"
#include "util.h"

typedef struct {
    int id_partida;
    int jugador_puma, jugador_perros;
    const ConfigCaso* cfg;
} ArgsJuez;

static void* rutina_juez(void* arg) {
    ArgsJuez* a = (ArgsJuez*)arg;
    const ConfigCaso* c = a->cfg;
    Mesa* m = calloc(1, sizeof(Mesa));
    Casilla* tablero = inicializar_tablero();
    m->id = a->id_partida;
    m->jugador_puma = a->jugador_puma;
    m->jugador_perros = a->jugador_perros;
    m->cfg = c;
    m->tablero = tablero;
    atomic_init(&m->activa, true);
    atomic_init(&m->abandono, ABANDONO_NINGUNO);
    atomic_init(&m->movimientos, 0);
    for (int i = 0; i < NUM_FICHAS; i++) {
        atomic_init(&m->pos[i], -1);
        atomic_init(&m->terminado[i], false);
    }
    colocar_iniciales(m);

    printf("[Mesa %d] Inicia: jugador %d (Puma) vs jugador %d (Perros)\n", m->id, m->jugador_puma,
           m->jugador_perros);

    for (int i = 0; i < NUM_FICHAS; i++) {
        Ficha* f = &m->fichas[i];
        f->mesa = m;
        f->id = i;
        f->n_instr = c->n_instr[i];
        f->cola = c->cola[i];
        f->burst = c->burst_total >= 0 ? c->burst_total / NUM_FICHAS + (i < c->burst_total % NUM_FICHAS ? 1 : 0)
                                       : c->burst_por_defecto;
        f->rs = c->seed + (unsigned)i + 1000u * (unsigned)m->id;
        pthread_create(&m->hilos[i], NULL, rutina_ficha, f);
    }

    int ultimo_mov = -1;
    while(true) {
        int ocup[TOTAL_CASILLAS];
        tomar_foto(m, ocup);
        int mov = atomic_load(&m->movimientos);
        if (mov != ultimo_mov) {
            ultimo_mov = mov;
        }

        int ab = atomic_load(&m->abandono);
        if (ab == ABANDONA_PUMA) {
            printf("[Mesa %d] El jugador Puma abandono: ganan los Perros por abandono\n", m->id);
            break;
        }
        if (ab == ABANDONA_PERROS) {
            printf("[Mesa %d] Un jugador de Perros abandono: gana el Puma por abandono\n", m->id);
            break;
        }
        int g = evaluar_victoria(m->tablero, ocup);
        if (g == 1) {
            printf("[Mesa %d] El Puma ha ganado (capturo %d perros)\n", m->id, CAPTURAS_VICTORIA);
            break;
        }
        if (g == 2) {
            printf("[Mesa %d] Los Perros han ganado (Puma acorralado)\n", m->id);
            break;
        }
        if (!servidor_corriendo) {
            printf("[Mesa %d] Partida interrumpida por SIGINT\n", m->id);
            break;
        }
        bool todos = true;
        for (int i = 0; i < NUM_FICHAS; i++)
            if (!atomic_load(&m->terminado[i])) {
                todos = false;
                break;
            }
        if (todos) {
            printf("[Mesa %d]No hay mas movimientos posibles, por defecto ganan los perros \n", m->id);
            break;
        }
        sleep_ms(POLL_JUEZ_MS);
    }

    atomic_store(&m->activa, false);
    for (int i = 0; i < NUM_FICHAS; i++) pthread_join(m->hilos[i], NULL);

    int ocup[TOTAL_CASILLAS];
    tomar_foto(m, ocup);

    destruir_tablero(m->tablero);
    free(m);
    free(a);
    sem_post(&mesas_disponibles);
    return NULL;
}

#endif
