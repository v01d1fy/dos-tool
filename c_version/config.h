#ifndef CONFIG_H
#define CONFIG_H

/*
 * DoS Testing Tool Configuration
 * Configure authorized targets before use
 */

/* SECURITY: Only these targets can be attacked (prevents misuse) */
/* Add your authorized test targets to this array */
static const char *AUTHORIZED_TARGETS[] = {
    /* "192.168.1.100", */
    /* "test.target.com", */
    /* "10.0.0.5", */
    NULL  /* sentinel */
};

/* Attack Safety Limits */
#define MAX_DURATION_SECONDS 300   /* Maximum 5 minutes per attack */
#define MAX_THREADS          1000  /* Maximum concurrent threads */
#define MAX_REQUESTS_PER_SECOND 10000

/* Default Attack Settings */
#define DEFAULT_THREADS  100
#define DEFAULT_DURATION 30
#define DEFAULT_PORT     80

/* Attack Types */
#define ATTACK_HTTP_FLOOD  1
#define ATTACK_SLOWLORIS   2
#define ATTACK_TCP_SYN     3
#define ATTACK_UDP_FLOOD   4

/* UDP Settings */
#define DEFAULT_PACKET_SIZE 1024
#define MAX_UDP_PAYLOAD     65507

/* Logging */
#define LOG_FILE "attacks.log"

#endif /* CONFIG_H */
