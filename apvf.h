#ifndef __APVF_H__
#define __APVF_H__

#include <pthread.h>
#include "theoraplay.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

typedef struct APList {
    const THEORAPLAY_AudioPacket *packet;
    struct APList                *next;
} APList;

typedef struct {
    APList         *head;
    APList         *last;
    uint            size;
    uint            channels;
    uint            freq;
    pthread_mutex_t mutex;
    pthread_cond_t  cond;
} AQueue;

extern AQueue aq;
extern int    quit;

void                          init_aq();
char                          aq_empty();
void                          aq_put(const THEORAPLAY_AudioPacket *packet);
const THEORAPLAY_AudioPacket *aq_get();
void                          aq_free();

#endif
