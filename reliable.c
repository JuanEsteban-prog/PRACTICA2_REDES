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
    // 1. Validar Checksum (si falla, return)
    
    // 2. Si es un paquete de DATOS:
    //    - Verificar si pkt->seqno == expected_seq_num
    //    - Si sí: ACCEPT_DATA, incrementar expected_seq_num
    //    - Siempre: Enviar ACK confirmando lo recibido
    
    // 3. Si es un paquete ACK:
    //    - Verificar si pkt->ackno > current_seq_num (o la lógica que uses)
    //    - Si confirma el actual: parar timer, avanzar current_seq_num, RESUME_TRANSMISSION
}

// Callback called when the application has data to be sent
void send_callback()
{
    // 1. Leer datos de la capa de aplicación (READ_DATA...)
    
    // 2. Construir paquete (seqno, data, longitud, tipo DATA)
    
    // 3. Calcular Checksum
    
    // 4. Enviar paquete
    
    // 5. Guardar copia en last_packet_sent
    
    // 6. Activar Timer (SET_TIMER)
    
    // 7. Pausar transmisión (PAUSE_TRANSMISSION) y marcar waiting_for_ack = 1
}

/*
    This function is called when timer timer_number expires.
*/
void timer_callback(int timer_number)
{
    // 1. Re-enviar last_packet_sent
    
    // 2. Reiniciar el Timer (SET_TIMER)
}