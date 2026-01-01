# Práctica 2: Implementación de un protocolo de control de flujo

La idea de esta práctica era coger un protocolo "inseguro" como UDP y construirle encima una capa de fiabilidad, básicamente haciendo lo que hace TCP pero a mano. Empecé el proyecto un poco cuesta arriba porque, siendo sincero, **no tenía mucha experiencia previa programando en C**. Al principio me costó adaptarme a la gestión manual de memoria y a los punteros, pero fue cuestión de pelearse con el código hasta entenderlo.

Lo primero que hice fue implementar el mecanismo básico de **Parada y Espera** (Stop & Wait). La lógica parecía sencilla: envío un paquete, pongo un temporizador y espero el "OK" (ACK) del receptor. Si no llega a tiempo, reenvío. Sin embargo, aquí me topé con el primer gran muro: mis paquetes llegaban siempre marcados como **"100% corruptos"**. Me volví un poco loco hasta descubrir que el problema no era mi lógica, sino la "basura" en la memoria RAM. Las estructuras de C tenían datos residuales que estropeaban el cálculo del *checksum*. Una vez aprendí a usar `memset` para limpiar todo antes de enviar, todo empezó a fluir.

Con la base funcionando, me animé a por la parte opcional: la **Ventana Deslizante** (*Go-Back-N*). Esto ya eran palabras mayores, porque implicaba no esperar confirmación por cada paquete, sino lanzar ráfagas continuas usando un buffer circular. Fue un reto de lógica interesante, sobre todo para coordinar qué paquetes estaban "en vuelo" y cuáles se podían borrar del buffer al recibir los ACKs acumulativos.

La prueba de fuego real fue cuando me puse a simular una red mala. Usando comandos de Linux (`tc`), le metí un retardo de **100ms** a mi propia máquina. Fue revelador ver la diferencia: mientras que mi primera versión se arrastraba lentísima esperando respuestas, la versión con Ventana Deslizante ni se inmutaba, llenando el canal de datos a toda velocidad.

Al final, me quedo con la satisfacción de haber pasado de pelearme con la sintaxis de C a tener un protocolo robusto capaz de recuperar paquetes perdidos y aguantar latencia real. Ha sido una de esas prácticas donde sufres al principio, pero aprendes un montón.
