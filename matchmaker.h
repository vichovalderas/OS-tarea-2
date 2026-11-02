#ifndef MATCHMAKER_H
#define MATCHMAKER_H

#include "cosas.h"
#include "config.h"
#include "estado.h"
#include "juez.h"
#include "util.h"

static atomic_int sigint_simulado_activo = 0;
static atomic_bool torneo_terminado;

typedef struct {
    pthread_mutex_t mtx;
    pthread_cond_t cv;
    int* q;
    int head, tail;
} ColaEspera;
static ColaEspera cola;

typedef struct {
    int id;
    unsigned int seed;
} ArgsJugador;

static void* rutina_jugador(void* arg) {
    ArgsJugador* j = (ArgsJugador*)arg;
    sleep_ms((int)((unsigned)rand_r(&j->seed) % 20u));
    if (!servidor_corriendo) return NULL;
    pthread_mutex_lock(&cola.mtx);
    cola.q[cola.tail++] = j->id;
    pthread_cond_signal(&cola.cv);
    pthread_mutex_unlock(&cola.mtx);
    return NULL;
}

static bool obtener_pareja(int* a, int* b) {
    pthread_mutex_lock(&cola.mtx);
    while (cola.tail - cola.head < 2) {
        if (!servidor_corriendo) {
            pthread_mutex_unlock(&cola.mtx);
            return false;
        }
        struct timespec ts;
        ms_a_abs(&ts, POLL_JUEZ_MS);
        pthread_cond_timedwait(&cola.cv, &cola.mtx, &ts);
    }
    if (!servidor_corriendo) {
        pthread_mutex_unlock(&cola.mtx);
        return false;
    }
    *a = cola.q[cola.head++];
    *b = cola.q[cola.head++];
    pthread_mutex_unlock(&cola.mtx);
    return true;
}

static bool esperar_mesa(void) {
    if (sem_trywait(&mesas_disponibles) == 0) return true;
    printf("[Matchmaker] Todas las mesas ocupadas, el par espera\n");
    while(true){
        if (!servidor_corriendo) return false;
        struct timespec ts;
        ms_a_abs(&ts, POLL_JUEZ_MS);
        if (sem_timedwait(&mesas_disponibles, &ts) == 0) return true;
        if (errno != ETIMEDOUT && errno != EINTR) return false;
    }
}

static void* rutina_senal(void* arg) {
    (void)arg;
    for (int t = 0; t < SIGINT_SIMULADO_MS; t += 5) {
        if (atomic_load(&torneo_terminado)) return NULL;
        sleep_ms(5);
    }
    if (atomic_load(&torneo_terminado)) return NULL;
    sigint_simulado_activo = 1;
    printf("[Test] Enviando SIGINT al servidor\n");
    kill(getpid(), SIGINT);
    return NULL;
}

static void ejecutar_torneo(const ConfigCaso* cfg, int N, int M) {
    if (cfg->spawn_jugadores > 0) N = cfg->spawn_jugadores;
    if (cfg->spawn_mesas > 0) M = cfg->spawn_mesas;
    int partidas = N / 2;

    printf("\nTEST_CASE %d: %s (jugadores=%d, mesas=%d)", cfg->caso, cfg->nombre, N, M);
    atomic_store(&torneo_terminado, false);

    sem_init(&mesas_disponibles, 0, (unsigned)M);
    pthread_mutex_init(&cola.mtx, NULL);
    pthread_cond_init(&cola.cv, NULL);
    cola.q = calloc((size_t)N, sizeof(int));
    cola.head = cola.tail = 0;

    pthread_t* jugadores = calloc((size_t)N, sizeof(pthread_t));
    ArgsJugador* aj = calloc((size_t)N, sizeof(ArgsJugador));
    pthread_t* jueces = calloc((size_t)(partidas > 0 ? partidas : 1), sizeof(pthread_t));
    pthread_t hilo_senal;
    bool senal_creada = false;

    if (cfg->sigint_simulado) {
        pthread_create(&hilo_senal, NULL, rutina_senal, NULL);
        senal_creada = true;
    }

    int creados = 0;
    for (int i = 0; i < N && servidor_corriendo; i++) {
        aj[i].id = i;
        aj[i].seed = cfg->seed + (unsigned)i;
        pthread_create(&jugadores[i], NULL, rutina_jugador, &aj[i]);
        creados++;
    }

    int lanzadas = 0;
    for (int p = 0; p < partidas; p++) {
        int a, b;
        if (!obtener_pareja(&a, &b)) break;
        printf("[Matchmaker] Par formado: jugador %d (Puma) + jugador %d (Perros)\n", a, b);
        if (!esperar_mesa()) break;

        ArgsJuez* aJ = malloc(sizeof(ArgsJuez));
        aJ->id_partida = p;
        aJ->jugador_puma = a;
        aJ->jugador_perros = b;
        aJ->cfg = cfg;
        pthread_create(&jueces[lanzadas], NULL, rutina_juez, aJ);
        lanzadas++;
    }

    if (!servidor_corriendo) {
        pthread_mutex_lock(&cola.mtx);
        pthread_mutex_unlock(&cola.mtx);
        printf("[Servidor] SIGINT: no se aceptan mas jugadores");
    }

    for (int i = 0; i < lanzadas; i++) pthread_join(jueces[i], NULL);
    for (int i = 0; i < creados; i++) pthread_join(jugadores[i], NULL);
    atomic_store(&torneo_terminado, true);
    if (senal_creada) pthread_join(hilo_senal, NULL);

    free(jueces);
    free(aj);
    free(jugadores);
    free(cola.q);
    cola.q = NULL;
    pthread_cond_destroy(&cola.cv);
    pthread_mutex_destroy(&cola.mtx);
    sem_destroy(&mesas_disponibles);

    if (sigint_simulado_activo) {
        sigint_simulado_activo = 0;
        servidor_corriendo = 1;
    }
}

#endif
