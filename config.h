#ifndef CONFIG_H
#define CONFIG_H

#include "cosas.h"

typedef enum { INS_MOVER, INS_CAPTURA, INS_DESCONECTAR } TipoInstr;

typedef struct {
    TipoInstr tipo;
    int origen_id;
    int destino_id;
    int presa_hint;
    int presa_ficha;
} Instruccion;

typedef struct {
    int caso;
    char nombre[96];
    unsigned int seed;
    int delay_min, delay_max;
    int puma_init;
    Instruccion cola[NUM_FICHAS][MAX_INSTR];
    int n_instr[NUM_FICHAS];
    int burst_total;
    int burst_por_defecto;
    int spawn_jugadores;
    int spawn_mesas;
    bool sigint_simulado;
} ConfigCaso;

static void config_por_defecto(ConfigCaso* c) {
    memset(c, 0, sizeof(*c));
    c->caso = 0;
    snprintf(c->nombre, sizeof c->nombre, "Configuracion por defecto");
    c->seed = 1;
    c->delay_min = 50;
    c->delay_max = 300;
    c->puma_init = 12;
    c->burst_total = -1;
}

static int coord_a_id(int x, int y) {
    if (x < 0 || x > 4 || y < 0 || y > 4) return -1;
    return y * 5 + x;
}

static int leer_pos(char* tk[], int n, int* i) {
    if (*i >= n) return -1;
    if (strcmp(tk[*i], "TRONO_TOP") == 0) { (*i)++; return TRONO_TOP; }
    if (strcmp(tk[*i], "TRONO_LEFT") == 0) { (*i)++; return TRONO_LEFT; }
    if (strcmp(tk[*i], "TRONO_RIGHT") == 0) { (*i)++; return TRONO_RIGHT; }
    if (*i + 1 >= n) { *i = n; return -1; }
    int x = atoi(tk[*i]), y = atoi(tk[*i + 1]);
    *i += 2;
    return coord_a_id(x, y);
}

static int parsear(char* linea, char* tk[], int max) {
    int n = 0;
    char* sp = NULL;
    for (char* t = strtok_r(linea, " \t\r\n", &sp); t && n < max; t = strtok_r(NULL, " \t\r\n", &sp)) tk[n++] = t;
    return n;
}

static void agregar_instr(ConfigCaso* c, int ficha, Instruccion in) {
    if (ficha < 0 || ficha >= NUM_FICHAS) {
        fprintf(stderr, "[config] ficha invalida (%d), evento ignorado\n", ficha);
        return;
    }
    if (c->n_instr[ficha] >= MAX_INSTR) {
        fprintf(stderr, "[config] demasiadas instrucciones para la ficha %d (max %d)\n", ficha, MAX_INSTR);
        return;
    }
    c->cola[ficha][c->n_instr[ficha]++] = in;
}

static void parse_config(char* tk[], int n, ConfigCaso* c) {
    for (int i = 1; i < n;) {
        if (strcmp(tk[i], "SEED") == 0 && i + 1 < n) { c->seed = (unsigned)strtoul(tk[i + 1], NULL, 10); i += 2; }
        else if (strcmp(tk[i], "DELAY_MIN") == 0 && i + 1 < n) { c->delay_min = atoi(tk[i + 1]); i += 2; }
        else if (strcmp(tk[i], "DELAY_MAX") == 0 && i + 1 < n) { c->delay_max = atoi(tk[i + 1]); i += 2; }
        else if (strcmp(tk[i], "PUMA_INIT") == 0 && i + 2 < n) {
            int id = coord_a_id(atoi(tk[i + 1]), atoi(tk[i + 2]));
            if (id >= 0) c->puma_init = id;
            i += 3;
        } else i++;
    }
    if (c->delay_min < 0) c->delay_min = 0;
    if (c->delay_max < c->delay_min) c->delay_max = c->delay_min;
}

static void parse_evento(char* tk[], int n, ConfigCaso* c) {
    if (n < 2) return;
    const char* ev = tk[1];
    if (strcmp(ev, "MOVE") == 0) {
        if (n < 7) return;
        int ficha;
        int idl = atoi(tk[3]);
        if (strcmp(tk[2], "PUMA") == 0) ficha = 0;
        else if (strcmp(tk[2], "DOG") == 0 && idl >= 0 && idl < NUM_PERROS) ficha = idl + 1;
        else { fprintf(stderr, "[config] ficha desconocida: %s %s\n", tk[2], tk[3]); return; }
        int i = 4;
        if (strcmp(tk[i], "FROM") != 0) return;
        i++;
        Instruccion in = {INS_MOVER, -1, -1, -1, -1};
        in.origen_id = leer_pos(tk, n, &i);
        if (i >= n || strcmp(tk[i], "TO") != 0) return;
        i++;
        in.destino_id = leer_pos(tk, n, &i);
        if (i < n && strcmp(tk[i], "CAPTURE") == 0) {
            in.tipo = INS_CAPTURA;
            i++;
            if (i + 1 < n && strcmp(tk[i], "DOG") == 0) {
                int pd = atoi(tk[i + 1]);
                if (pd >= 0 && pd < NUM_PERROS) in.presa_ficha = pd + 1;
                i += 2;
            }
            if (i < n && strcmp(tk[i], "AT") == 0) {
                i++;
                in.presa_hint = leer_pos(tk, n, &i);
            }
        }
        agregar_instr(c, ficha, in);
    } else if (strcmp(ev, "DISCONNECT") == 0 && n >= 3) {
        Instruccion in = {INS_DESCONECTAR, -1, -1, -1, -1};
        if (strcmp(tk[2], "PLAYER_PUMA") == 0) agregar_instr(c, 0, in);
        else if (strcmp(tk[2], "PLAYER_DOG") == 0 && n >= 5) {
            int d = atoi(tk[4]);
            if (d >= 0 && d < NUM_PERROS) agregar_instr(c, d + 1, in);
        }
    } else if (strcmp(ev, "SPAWN_PLAYERS") == 0) {
        for (int i = 2; i + 1 < n; i++) {
            if (strcmp(tk[i], "COUNT") == 0) c->spawn_jugadores = atoi(tk[i + 1]);
            if (strcmp(tk[i], "MESAS_MAX") == 0) c->spawn_mesas = atoi(tk[i + 1]);
        }
    } else if (strcmp(ev, "WAIT_QUEUED_PLAYERS") == 0) {

    } else if (strcmp(ev, "SIGNAL_SIGINT") == 0) {
        c->sigint_simulado = true;
    } else if (strcmp(ev, "BURST_RANDOM_MOVES") == 0) {
        for (int i = 2; i + 1 < n; i++)
            if (strcmp(tk[i], "COUNT") == 0) c->burst_total = atoi(tk[i + 1]);
    }
}

static int cargar_caso(const char* archivo, int caso, ConfigCaso* c) {
    FILE* f = fopen(archivo, "r");
    if (!f) return -1;
    config_por_defecto(c);

    char linea[512], copia[512];
    bool dentro = false, encontrado = false;
    while (fgets(linea, sizeof linea, f)) {
        strcpy(copia, linea);
        char* tk[32];
        int n = parsear(copia, tk, 32);
        if (n == 0) continue;

        if (strcmp(tk[0], "TEST_CASE") == 0 && n >= 2) {
            if (dentro) break;
            if (atoi(tk[1]) == caso) {
                dentro = encontrado = true;
                c->caso = caso;
                char* q1 = strchr(linea, '"');
                char* q2 = strrchr(linea, '"');
                if (q1 && q2 && q2 > q1) {
                    size_t len = (size_t)(q2 - q1 - 1);
                    if (len >= sizeof c->nombre) len = sizeof c->nombre - 1;
                    memcpy(c->nombre, q1 + 1, len);
                    c->nombre[len] = '\0';
                }
            }
            continue;
        }
        if (!dentro) continue;
        if (strcmp(tk[0], "TEST_END") == 0) break;
        if (strcmp(tk[0], "CONFIG") == 0) parse_config(tk, n, c);
        else if (strcmp(tk[0], "EVENT") == 0) parse_evento(tk, n, c);
    }
    fclose(f);
    if (!encontrado) return -2;

    bool con_guion = false;
    for (int i = 0; i < NUM_FICHAS; i++)
        if (c->n_instr[i] > 0) con_guion = true;
    if (!con_guion && c->burst_total < 0) {
        int prom = (c->delay_min + c->delay_max) / 2;
        if (prom < 1) prom = 1;
        c->burst_por_defecto = 1000 / prom;
        if (c->burst_por_defecto < 3) c->burst_por_defecto = 3;
    }
    return 0;
}

static int listar_casos(const char* archivo, int* out, int max) {
    FILE* f = fopen(archivo, "r");
    if (!f) return -1;
    char linea[512];
    int n = 0;
    while (fgets(linea, sizeof linea, f) && n < max) {
        char* tk[8];
        char copia[512];
        strcpy(copia, linea);
        int nt = parsear(copia, tk, 8);
        if (nt >= 2 && strcmp(tk[0], "TEST_CASE") == 0) out[n++] = atoi(tk[1]);
    }
    fclose(f);
    return n;
}

#endif
