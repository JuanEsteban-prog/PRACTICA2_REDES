#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stddef.h>
#include <assert.h>
#include <poll.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <netinet/in.h>

#include "rlib.h"

// --- CONSTANTES ---
// Usamos MAX_PAYLOAD si existe, si no, definimos un valor seguro
#ifndef MAX_PAYLOAD
#define MAX_PAYLOAD 500
#endif

// --- VARIABLES GLOBALES ---
static char last_payload[MAX_PAYLOAD]; // Buffer para guardar los datos
static int last_payload_size;          // Tamaño de los datos guardados

static int waiting_for_ack;
static uint32_t next_seqno;
static uint32_t expected_seqno;
static long timeout_val;

/*
 * INICIALIZACIÓN
 */
void connection_initialization(int window_size, long timeout_in_ns)
{
    waiting_for_ack = 0;
    next_seqno = 1;     // Empezamos secuencia en 1
    expected_seqno = 1; // Esperamos recibir el 1
    timeout_val = timeout_in_ns;
    last_payload_size = 0;

    memset(last_payload, 0, MAX_PAYLOAD);
}

/*
 * RECEPCIÓN
 */
void receive_callback(packet_t *pkt, size_t pkt_size)
{

    // 1. VALIDAR INTEGRIDAD
    // Si la macro dice que está mal, lo ignoramos.
    if (VALIDATE_CHECKSUM(pkt) == 0)
    {
        return;
    }

    // 2. IDENTIFICAR TIPO DE PAQUETE
    // Usamos la macro IS_ACK_PACKET para no liarnos con tamaños
    if (IS_ACK_PACKET(pkt))
    {

        // Convertimos el ackno a formato host para comparar
        uint32_t ack_val = ntohl(pkt->ackno);

        // Si es el ACK que esperamos (confirma el que acabamos de enviar)
        if (waiting_for_ack && ack_val >= (next_seqno - 1))
        {
            SET_TIMER(0, -1); // Apagar timer
            waiting_for_ack = 0;
            RESUME_TRANSMISSION();
        }
    }
    else
    {
        // ES UN PAQUETE DE DATOS
        uint32_t seq_val = ntohl(pkt->seqno);

        // ¿Es el número de secuencia exacto que esperamos?
        if (seq_val == expected_seqno)
        {

            // Calculamos longitud de datos
            int data_len = ntohs(pkt->len) - DATA_PACKET_HEADER;

            if (data_len > 0)
            {
                if (data_len > MAX_PAYLOAD)
                    data_len = MAX_PAYLOAD;
                ACCEPT_DATA(pkt->data, data_len);
            }
            expected_seqno++;
        }

        // SIEMPRE ENVIAR ACK (CONFIRMACIÓN ACUMULATIVA)
        // Usamos la macro que calcula checksums y cabeceras sola
        SEND_ACK_PACKET(expected_seqno);
    }
}

/*
 * ENVÍO
 */
void send_callback()
{

    if (waiting_for_ack)
    {
        PAUSE_TRANSMISSION();
        return;
    }

    // Leemos datos de la aplicación
    int bytes_read = READ_DATA_FROM_APP_LAYER(last_payload, MAX_PAYLOAD);

    if (bytes_read > 0)
    {
        last_payload_size = bytes_read;

        // --- ENVIAR USANDO LA MACRO ---
        // SEND_DATA_PACKET(longitud_total, ackno, seqno, puntero_datos)
        // La macro se encarga de htons, htonl, checksum y memset.
        SEND_DATA_PACKET(bytes_read + DATA_PACKET_HEADER, 0, next_seqno, last_payload);

        SET_TIMER(0, timeout_val);
        next_seqno++;
        waiting_for_ack = 1;
        PAUSE_TRANSMISSION();
    }
}

/*
 * RETRANSMISIÓN
 */
void timer_callback(int timer_number)
{
    if (timer_number == 0 && waiting_for_ack)
    {

        // REENVIAR EL ÚLTIMO PAQUETE
        // Nota: Enviamos (next_seqno - 1) porque next_seqno ya avanzó
        SEND_DATA_PACKET(last_payload_size + DATA_PACKET_HEADER, 0, next_seqno - 1, last_payload);

        SET_TIMER(0, timeout_val);
    }
}