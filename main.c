#include "cosas.h"
#include "estado.h"
#include "config.h"
#include "matchmaker.h"

static void manejador_sigint(int sig) {
    (void)sig;
    int errno_guardado = errno;
    servidor_corriendo = 0;
    const char msg[] = "\n[SIGINT] Apagando el servidor \n";
    ssize_t r = write(STDOUT_FILENO, msg, sizeof(msg) - 1);
    (void)r;
    errno = errno_guardado;
}

int main(int argc, char* argv[]) {
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = manejador_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    setvbuf(stdout, NULL, _IOLBF, 0);

    if (argc < 3) {
        printf("Uso: %s N M [config.txt [caso]]\n", argv[0]);
        return 1;
    }
    int N = atoi(argv[1]);
    int M = atoi(argv[2]);
    if (N < 2 || M < 1) {
        fprintf(stderr, "N debe ser >= 2 y M >= 1\n");
        return 1;
    }

    ConfigCaso* cfg = malloc(sizeof(ConfigCaso));
    if (!cfg) return 1;

    int rc = 0;
    if (argc < 4) {
        config_por_defecto(cfg);
        cfg->burst_por_defecto = 20;
        ejecutar_torneo(cfg, N, M);
    } else {
        int casos[MAX_CASOS];
        int nc = 0;
        if (argc >= 5) {
            casos[nc++] = atoi(argv[4]);
        } else {
            nc = listar_casos(argv[3], casos, MAX_CASOS);
            if (nc < 0) {
                fprintf(stderr, "No se pudo abrir %s\n", argv[3]);
                rc = 1;
            } else if (nc == 0) {
                fprintf(stderr, "%s no contiene TEST_CASE\n", argv[3]);
                rc = 1;
            }
        }
        for (int i = 0; i < nc && servidor_corriendo && rc == 0; i++) {
            int r = cargar_caso(argv[3], casos[i], cfg);
            if (r == -1) {
                fprintf(stderr, "No se pudo abrir %s\n", argv[3]);
                rc = 1;
            } else if (r == -2) {
                fprintf(stderr, "Caso %d no encontrado en %s\n", casos[i], argv[3]);
                rc = 1;
            } else {
                ejecutar_torneo(cfg, N, M);
            }
        }
    }

    free(cfg);
    printf("\nTorneo finalizado .\n");
    return rc;
}
