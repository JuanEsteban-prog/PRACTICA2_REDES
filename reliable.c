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

// --- SECCIÓN 2: VARIABLES GLOBALES ---

// Estructura para guardar el último paquete enviado (para retransmisión)
packet_t last_packet_sent;
int last_packet_len;

// Variables de control de flujo
int current_seq_num;  // EMISOR: Mi número de secuencia actual a enviar
int expected_seq_num; // RECEPTOR: El número de secuencia que espero recibir
int waiting_for_ack;  // EMISOR: 1 si estoy esperando ACK, 0 si puedo enviar

// Guardar el timeout configurado
long saved_timeout;

/*
    Callback functions: The following functions are called on the corresponding
    event as explained in rlib.h file. You should implement these functions.
*/

/*
    Creates a new connection. 
*/
void connection_initialization(int window_size, long timeout_in_ns)
{
    // CORRECCIÓN: El código debe ir DENTRO de las llaves
    current_seq_num = 0;
    expected_seq_num = 0; // Empezamos esperando el paquete 0
    waiting_for_ack = 0;
    saved_timeout = timeout_in_ns;
    
    // Opcional: imprimir para depurar
    // printf("Inicialización completa. Timeout: %ld\n", saved_timeout);
}

// This callback is called when a packet pkt of size pkt_size is received
void receive_callback(packet_t *pkt, size_t pkt_size)
{
    if (!VALIDATE_CHECKSUM(pkt, pkt_size)) {
        return;
    }

    // Convertir campos de red a host para poder leerlos correctamente
    uint32_t seq_recibido = ntohl(pkt->seqno);
    uint32_t ack_recibido = ntohl(pkt->ackno);
    uint16_t longitud = ntohs(pkt->length);

    // --- CASO A: ES UN PAQUETE ACK ---
    // Sabemos que es un ACK si su tamaño es ACK_PACKET_SIZE
    if (pkt_size == ACK_PACKET_SIZE) {
        
        // Verificamos si estamos esperando un ACK y si el número es correcto.
        // En Stop&Wait, el ACK suele indicar el SIGUIENTE paquete que espera (RR n+1)
        // Por tanto, si envié el 0, espero un ACK con ackno=1.
        if (waiting_for_ack == 1 && ack_recibido == (current_seq_num + 1)) {
            
            // ¡Confirmación correcta!
            CANCEL_TIMER(0);           // Apagar la alarma
            waiting_for_ack = 0;       // Ya no esperamos
            current_seq_num++;         // Avanzamos al siguiente número de secuencia
            RESUME_TRANSMISSION();     // Permitimos a la app enviar más datos
        }
        return; // Terminamos
    }

    // --- CASO B: ES UN PAQUETE DE DATOS ---
    // Si llegamos aquí, es un paquete con datos
    
    // 1. Verificar si es el paquete que esperamos
    if (seq_recibido == expected_seq_num) {
        // Es correcto y en orden. Entregamos los datos a la aplicación.
        // Restamos el tamaño de la cabecera para saber cuántos bytes de datos hay
        int data_size = longitud - DATA_PACKET_HEADER;
        ACCEPT_DATA(pkt->data, data_size);
        
        // Incrementamos el contador de lo que esperamos recibir la próxima vez
        expected_seq_num++;
    }
    
    // 2. Enviar ACK (Confirmación)
    // SIEMPRE enviamos ACK. Si el paquete era repetido (seq_recibido < expected),
    // enviamos el ACK igual para que el emisor sepa que ya lo tenemos y deje de retransmitir.
    
    packet_t ack_pkt;
    ack_pkt.seqno = 0; // En un ACK puro, el seqno no suele importar
    ack_pkt.ackno = htonl(expected_seq_num); // Confirmamos que esperamos el siguiente
    ack_pkt.length = htons(ACK_PACKET_SIZE);
    
    ack_pkt.checksum = 0;
    ack_pkt.checksum = CHECKSUM_CRC(&ack_pkt, ACK_PACKET_SIZE);
    
    SEND_PACKET(&ack_pkt, ACK_PACKET_SIZE);
}

// Callback called when the application has data to be sent
void send_callback()
{
    // 1. Preparar un buffer para el paquete
    packet_t pkt;
    int data_len;

    // 2. Leer datos de la capa de aplicación
    // MAX_DATA_SIZE es una constante definida en rlib.h
    data_len = READ_DATA_FROM_APP_LAYER(pkt.data, MAX_DATA_SIZE);

    // 3. Configurar la cabecera del paquete
    pkt.seqno = htonl(current_seq_num); // Convertir a formato de red (Big Endian)
    pkt.ackno = htonl(0);               // No es un ACK, ponemos 0
    // La longitud total es la cabecera + los datos leídos
    pkt.length = htons(DATA_PACKET_HEADER + data_len); 
    
    // 4. Calcular Checksum (IMPORTANTE: Poner a 0 antes de calcular)
    pkt.checksum = 0;
    pkt.checksum = CHECKSUM_CRC(&pkt, DATA_PACKET_HEADER + data_len);

    // 5. Guardar copia en las variables globales (para retransmisión)
    memcpy(&last_packet_sent, &pkt, sizeof(packet_t));
    last_packet_len = DATA_PACKET_HEADER + data_len;

    // 6. Enviar el paquete a la red
    SEND_PACKET(&pkt, last_packet_len);

    // 7. Configurar el Timer y el estado
    // Usamos el timer 0. Si no llega ACK en 'saved_timeout' ns, saltará timer_callback
    SET_TIMER(0, saved_timeout);
    
    // 8. Detener transmisión (Stop & Wait: no enviamos más hasta recibir ACK)
    waiting_for_ack = 1;
    PAUSE_TRANSMISSION();

    // Depuración (opcional)
    // printf("Enviado paquete SEQ: %d. Esperando ACK.\n", current_seq_num);
}

/*
    This function is called when timer timer_number expires.
*/
void timer_callback(int timer_number)
{
  if (timer_number == 0) {
        // 1. Re-enviar el último paquete guardado
        SEND_PACKET(&last_packet_sent, last_packet_len);

        // 2. Volver a activar el timer
        SET_TIMER(0, saved_timeout);

        // Depuración
        // printf("Timeout! Retransmitiendo paquete SEQ: %d\n", current_seq_num);
    }
}