#ifndef MESA_H
#define MESA_H

#include "cosas.h"
#include "config.h"
#include "tablero.h"
#include "util.h"

enum { ABANDONO_NINGUNO = 0, ABANDONA_PUMA = 1, ABANDONA_PERROS = 2 };

struct Mesa;
typedef struct {
    struct Mesa* mesa;
    int id;
    int n_instr;
    const Instruccion* cola;
    int burst;
    unsigned int rs;
} Ficha;

typedef struct Mesa {
    int id;
    int jugador_puma, jugador_perros;
    const ConfigCaso* cfg;
    Casilla* tablero;
    atomic_bool activa;
    atomic_int pos[NUM_FICHAS];
    atomic_bool terminado[NUM_FICHAS];
    atomic_int abandono;
    atomic_int movimientos;
    Ficha fichas[NUM_FICHAS];
    pthread_t hilos[NUM_FICHAS];
} Mesa;

static void dormir_ms(Mesa* m, int ms) {
    while (ms > 0 && atomic_load(&m->activa)) {
        int rebanada = ms > 5 ? 5 : ms;
        dormir_ms_simple(rebanada);
        ms -= rebanada;
    }
}

static void tomar_foto(Mesa* m, int ocup[TOTAL_CASILLAS]) {
    for (int i = 0; i < TOTAL_CASILLAS; i++) pthread_mutex_lock(&m->tablero[i].mutex);
    for (int i = 0; i < TOTAL_CASILLAS; i++) ocup[i] = atomic_load(&m->tablero[i].ocupante);
    for (int i = TOTAL_CASILLAS - 1; i >= 0; i--) pthread_mutex_unlock(&m->tablero[i].mutex);
}

static const char* ejecutar_movimiento(Mesa* m, int yo, const Instruccion* in) {
    Casilla* t = m->tablero;
    Casilla *origen = NULL, *destino = NULL, *presa = NULL;
    const char* err = NULL;

    if (in->origen_id < 0 || in->origen_id >= TOTAL_CASILLAS || in->destino_id < 0 ||
        in->destino_id >= TOTAL_CASILLAS) {
        err = "casilla inexistente";
    } else {
        origen = &t[in->origen_id];
        destino = &t[in->destino_id];
        if (in->tipo == INS_CAPTURA) {
            int mid = obtener_casilla_intermedia(origen->id, destino->id);
            if (yo != 0) err = "solo el Puma puede capturar";
            else if (mid == -1 || (in->presa_hint >= 0 && in->presa_hint != mid)) err = "salto ilegal";
            else presa = &t[mid];
        } else if (!es_adyacente(origen, destino)) {
            err = "casillas no adyacentes";
        }
    }

    if (!err) {

        Casilla* locks[3];
        int nl = 0;
        locks[nl++] = origen;
        locks[nl++] = destino;
        if (presa) locks[nl++] = presa;
        for (int i = 0; i < nl - 1; i++)
            for (int j = 0; j < nl - i - 1; j++)
                if (locks[j]->id > locks[j + 1]->id) {
                    Casilla* tmp = locks[j];
                    locks[j] = locks[j + 1];
                    locks[j + 1] = tmp;
                }
        for (int i = 0; i < nl; i++) pthread_mutex_lock(&locks[i]->mutex);

        if (atomic_load(&origen->ocupante) != yo) err = "la ficha ya no esta en el origen";
        else if (atomic_load(&destino->ocupante) != LIBRE) err = "colision: destino ocupado";
        else if (presa && atomic_load(&presa->ocupante) < 1) err = "no hay perro que capturar";

        if (!err) {
            atomic_store(&destino->ocupante, yo);
            atomic_store(&origen->ocupante, LIBRE);
            atomic_store(&m->pos[yo], destino->id);
            int victima = -1;
            if (presa) {
                victima = atomic_load(&presa->ocupante);
                atomic_store(&presa->ocupante, LIBRE);
                atomic_store(&m->pos[victima], -1);
            }
            int nmov = atomic_fetch_add(&m->movimientos, 1) + 1;
            char f[24], o[16], d[16];
            nombre_ficha(yo, f, sizeof f);
            nombre_casilla(origen->id, o, sizeof o);
            nombre_casilla(destino->id, d, sizeof d);
            if (presa) {
                char p[16], v[24];
                nombre_casilla(presa->id, p, sizeof p);
                nombre_ficha(victima, v, sizeof v);
                printf("[Mesa %d] #%d %s salto %s -> %s y capturo a %s en %s\n", m->id, nmov, f, o, d, v, p);
            } else {
                printf("[Mesa %d] #%d %s se movio %s -> %s\n", m->id, nmov, f, o, d);
            }
        }

        for (int i = nl - 1; i >= 0; i--) pthread_mutex_unlock(&locks[i]->mutex);
    }

    if (err) {
        char f[24], o[16], d[16];
        nombre_ficha(yo, f, sizeof f);
        nombre_casilla(in->origen_id, o, sizeof o);
        nombre_casilla(in->destino_id, d, sizeof d);
        printf("[Mesa %d] RECHAZADO %s %s -> %s: %s\n", m->id, f, o, d, err);
    }
    return err;
}

static bool elegir_aleatorio(Mesa* m, int yo, unsigned int* rs, Instruccion* out) {
    int p = atomic_load(&m->pos[yo]);
    if (p < 0) return false;
    Casilla* t = m->tablero;
    Instruccion mov[MAX_VECINOS];
    Instruccion cap[TOTAL_CASILLAS];
    int nm = 0, nc = 0;

    for (int i = 0; i < t[p].num_vecinos; i++) {
        int v = t[p].vecinos[i]->id;
        if (atomic_load(&t[v].ocupante) == LIBRE) {
            Instruccion x = {INS_MOVER, p, v, -1, -1};
            mov[nm++] = x;
        }
    }
    if (yo == 0) {
        for (int d = 0; d < TOTAL_CASILLAS; d++) {
            if (atomic_load(&t[d].ocupante) != LIBRE) continue;
            int mid = obtener_casilla_intermedia(p, d);
            if (mid != -1 && atomic_load(&t[mid].ocupante) >= 1) {
                Instruccion x = {INS_CAPTURA, p, d, mid, -1};
                cap[nc++] = x;
            }
        }
    }
    if (nc > 0 && (nm == 0 || rand_r(rs) % 3 != 0)) {
        *out = cap[(unsigned)rand_r(rs) % (unsigned)nc];
        return true;
    }
    if (nm > 0) {
        *out = mov[(unsigned)rand_r(rs) % (unsigned)nm];
        return true;
    }
    return false;
}

static void* rutina_ficha(void* arg) {
    Ficha* f = (Ficha*)arg;
    Mesa* m = f->mesa;
    int yo = f->id;
    char nom[24];
    nombre_ficha(yo, nom, sizeof nom);
    int rango = m->cfg->delay_max - m->cfg->delay_min + 1;

    for (int paso = 0; atomic_load(&m->activa); paso++) {
        if (atomic_load(&m->pos[yo]) < 0) break;

        Instruccion in;
        bool aleatorio = false;
        if (paso < f->n_instr) {
            in = f->cola[paso];
            if (in.tipo == INS_DESCONECTAR) {

                int esperado = ABANDONO_NINGUNO;
                atomic_compare_exchange_strong(&m->abandono, &esperado,
                                               yo == 0 ? ABANDONA_PUMA : ABANDONA_PERROS);
                printf("[Mesa %d] %s se DESCONECTO abruptamente\n", m->id, nom);
                break;
            }
        } else if (f->burst > 0) {
            f->burst--;
            aleatorio = true;
        } else {
            break;
        }

        dormir_ms(m, m->cfg->delay_min + (int)((unsigned)rand_r(&f->rs) % (unsigned)rango));
        if (!atomic_load(&m->activa)) break;

        if (aleatorio && !elegir_aleatorio(m, yo, &f->rs, &in)) continue;
        ejecutar_movimiento(m, yo, &in);
    }
    atomic_store(&m->terminado[yo], true);
    return NULL;
}

static int evaluar_victoria(const Casilla* t, const int ocup[TOTAL_CASILLAS]) {
    int vivos = 0, puma = -1;
    for (int i = 0; i < TOTAL_CASILLAS; i++) {
        if (ocup[i] >= 1) vivos++;
        if (ocup[i] == 0) puma = i;
    }
    if (NUM_PERROS - vivos >= CAPTURAS_VICTORIA) return 1;
    if (puma == -1) return 0;

    for (int i = 0; i < t[puma].num_vecinos; i++)
        if (ocup[t[puma].vecinos[i]->id] == LIBRE) return 0;
    for (int d = 0; d < TOTAL_CASILLAS; d++) {
        if (ocup[d] != LIBRE) continue;
        int mid = obtener_casilla_intermedia(puma, d);
        if (mid != -1 && ocup[mid] >= 1) return 0;
    }
    return 2;
}

static void colocar_iniciales(Mesa* m) {
    const ConfigCaso* c = m->cfg;
    int destino[NUM_FICHAS];
    bool ocupada[TOTAL_CASILLAS] = {false};
    for (int i = 0; i < NUM_FICHAS; i++) destino[i] = -1;

    destino[0] = c->puma_init;
    ocupada[c->puma_init] = true;

    for (int d = 1; d < NUM_FICHAS; d++) {
        if (c->n_instr[d] == 0) continue;
        const Instruccion* i0 = &c->cola[d][0];
        if (i0->tipo != INS_DESCONECTAR && i0->origen_id >= 0 && !ocupada[i0->origen_id]) {
            destino[d] = i0->origen_id;
            ocupada[i0->origen_id] = true;
        }
    }
    for (int f = 0; f < NUM_FICHAS; f++) {
        for (int k = 0; k < c->n_instr[f]; k++) {
            const Instruccion* in = &c->cola[f][k];
            if (in->tipo != INS_CAPTURA || in->presa_hint < 0 || ocupada[in->presa_hint]) continue;
            int cand = -1;
            if (in->presa_ficha >= 1 && in->presa_ficha < NUM_FICHAS && destino[in->presa_ficha] == -1)
                cand = in->presa_ficha;
            for (int d = 1; d < NUM_FICHAS && cand == -1; d++)
                if (destino[d] == -1) cand = d;
            if (cand != -1) {
                destino[cand] = in->presa_hint;
                ocupada[in->presa_hint] = true;
            }
        }
    }
    int cursor = 0;
    for (int d = 1; d < NUM_FICHAS; d++) {
        if (destino[d] != -1) continue;
        while (cursor < TOTAL_CASILLAS && ocupada[cursor]) cursor++;
        if (cursor >= TOTAL_CASILLAS) break;
        destino[d] = cursor;
        ocupada[cursor] = true;
    }
    for (int i = 0; i < NUM_FICHAS; i++) {
        atomic_store(&m->pos[i], destino[i]);
        if (destino[i] >= 0) atomic_store(&m->tablero[destino[i]].ocupante, i);
    }
}

#endif
