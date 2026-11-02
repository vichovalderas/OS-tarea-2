#ifndef ESTADO_H
#define ESTADO_H

#include "cosas.h"

static atomic_int servidor_corriendo = 1;

static sem_t mesas_disponibles;

#endif
