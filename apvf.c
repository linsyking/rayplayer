#include "apvf.h"

/* ALL GLOBAL VARIABLES */
AQueue aq;
int    quit = 0;
/* ALL GLOBAL VARIABLES */

void init_aq() {
    aq.head     = NULL;
    aq.last     = NULL;
    aq.size     = 0;
    aq.channels = 0;
    aq.freq     = 0;
    pthread_mutex_init(&aq.mutex, NULL);
    pthread_cond_init(&aq.cond, NULL);
}

char aq_empty() {
    return aq.size == 0;
}

void aq_put(const THEORAPLAY_AudioPacket *packet) {
    pthread_mutex_lock(&aq.mutex);
    APList *pl = malloc(sizeof(APList));
    pl->packet = packet;
    pl->next   = NULL;
    if (aq_empty()) {
        aq.head = pl;
        aq.last = pl;
    } else {
        aq.last->next = pl;
        aq.last       = pl;
    }
    aq.size++;
    pthread_cond_signal(&aq.cond);
    pthread_mutex_unlock(&aq.mutex);
}

const THEORAPLAY_AudioPacket *aq_get() {
    pthread_mutex_lock(&aq.mutex);
    while (aq_empty() && quit == 0) {
        pthread_cond_wait(&aq.cond, &aq.mutex);
    }
    if (quit) {
        return NULL;
    }
    APList *node                    = aq.head;
    aq.head                         = aq.head->next;
    const THEORAPLAY_AudioPacket *p = node->packet;
    free(node);
    if (aq.head == NULL) {
        aq.last = NULL;
    }
    aq.size--;
    pthread_mutex_unlock(&aq.mutex);
    return p;
}

void aq_free() {
    pthread_mutex_destroy(&aq.mutex);
    pthread_cond_destroy(&aq.cond);
    // Free all nodes
    printf("total %d audio packets found, destroying them...\n", aq.size);
    APList *node = aq.head;
    while (node != NULL) {
        APList *next = node->next;
        THEORAPLAY_freeAudio(node->packet);
        free(node);
        node = next;
    }
}
