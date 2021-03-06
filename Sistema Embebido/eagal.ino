// This #include statement was automatically added by the Particle IDE.
#include "Eeprom.h"

// This #include statement was automatically added by the Particle IDE.
// COLORES:
// Steel Blue
// Sky Blue
// Powder Blue
#include "Audio.h"

#include <PietteTech_DHT.h>
#include "Pantalla.h"

#define DHTTYPE DHT11
#define DHTPIN 5

#define POWDER_BLUE 46876
#define SKY_BLUE 34429
#define STEEL_BLUE 17430
#define GREY 50712

/*#define DIR_HORA_ALARMA 0
#define DIR_LAT 5
#define DIR_LON 9
#define DIR_ALARMA_ACTIVA 13
#define DIR_CANCION 17*/

PietteTech_DHT dht(DHTPIN, DHTTYPE); // OBJETO DE LA LIBRERIA PARA USAR DHT11

int pFotoresistor = A0; // PIN DE ENTRADA DEL FOTORESISTOR (DEL CUAL SE LEE EL VALOR)

int fFotoresistor = A5; // PIN DE ENERGIA DEL FOTORESISTOR (ENTREGA CORRIENTE)

int vFotoresistor; // GLOBAL CON EL VALOR LEIDO EN EL PIN A0

int pBoton = D6; // PIN DE BOTON DE INTERRUPCION - DETECTA FLANCO DE BAJADA

int temp = -1;
int hum = -1;

int cancionElegida;

String nivel_luz; // GLOBAL PARA EL NIVEL DE LUZ DE PANTALLA
String nivel_luz_anterior; // GLOBAL PARA EL NIVEL DE LUZ ANTERIOR (PARA NO ENVIAR DATOS INNECESARIOS A LA PANTALLA)

volatile int estado = 0; // GLOBAL DEL ESTADO DE LA MAQUINA DE ESTADOS.

String hora = ""; // GLOBAL DE HORA ACTUAL
String horaAlarma; // GLOBAL DE HORA DE ALARMA
String fecha; // GLOBAL DE FECHA ACTUAL
String fecha_anterior; // GLOBAL DE FECHA ANTERIOR EN EL LOOP (PARA NO ENVIAR DATOS INNECESARIOS A LA PANTALLA)
int alarmaActiva = 0;
int sonando = 0;

// AREA DE TIMERS1
Timer tFechaHora(500, actualizarFechaHora); // TIMER QUE ACTUALIZA LA FECHA Y LA HORA CADA 500 ms
Timer tTemperaturaHumedad(600000, actualizarTemperaturaHumedad);
Timer tPronostico(7200000, obtenerPronostico);
Timer tNivelLuz(1000, actualizarNivelLuz);
Timer tCotizacion(86400000, obtenerCotizacion);
// FIN AREA DE TIMERS

Audio buzzer(D0);

String ubicacion;

int brilloLDR = 1;

void setup() {
    
    // LEE LATITUD Y LONGITUD DE LA MEMORIA EEPROM
    ubicacion = Eeprom::obtenerUbicacion();
    
    // LEE HORA DE ALARMA DE MEMORIA EEPROM
    horaAlarma = Eeprom::obtenerHoraAlarma();
    
    // LEE SI LA ALARMA DEBE ESTAR ACTIVADA DE MEMORIA EEPROM
    alarmaActiva = Eeprom::obtenerEstadoAlarma();
    
    // LEE LA CANCION SELECCIONADA DE MEMORIA EEPROM
    cancionElegida = Eeprom::obtenerCancion();
    buzzer.setCancion(cancionElegida);
    
    // VARIBALES REGISTRADAS RESTFUL
    Particle.variable("temperatura", temp); // Temperatura del sensor
    Particle.variable("humedad", hum); // Humedad del sensor
    Particle.variable("horaalarma", horaAlarma); // Hora que suena la alarma (Formato HH:MM 24H)
    Particle.variable("alarmaActiva", alarmaActiva);  // True: La alarma suena a la hora configurada. False: La alarma no suena a la hora configurada
    Particle.variable("sonando", sonando); // True: La alarma esta sonando. False: La alarma no esta sonando.
    Particle.variable("brilloLDR", brilloLDR);
    Particle.variable("cancion", cancionElegida);
    //
    
    // EVENTOS SUSCRIPTOS DE WEBHOOK PARA OBTENER DATOS DE LOS WEBSERVICES
    Particle.subscribe("hook-response/pronostico", actualizarPronostico, MY_DEVICES);
    Particle.subscribe("hook-response/cotizacion", actualizarCotizacion, MY_DEVICES);
    //
    
    // FUNCIONES DISPONIBLES RESTFUL
    Particle.function("setAlarma", setAlarma); // Cambiar hora de alarma. Formato HH:MM 24H.
    Particle.function("setCancion", setCancion); // Cambiar canción de alarma. Valores [0-3].
    Particle.function("setBrillo", setBrillo); // Cambiar brillo de la pantalla. Valores [0-100].
    Particle.function("setUbicacion", setUbicacion); // Cambiar ubicación para el webservice de pronosticos. Valores: String para hacer query de búsqueda.
    Particle.function("toggleBrillo", toggleBrillo); // Enviarle 0 para que la pantalla controle el brillo con el LDR. Enviarle 1 para utilizar el brillo enviado por el webservice.
    Particle.function("apagarAlarma", apagarAlarma); // Llamar con cualquier argumento para apagar alarma.
    Particle.function("toggleAlarma", toggleAlarma); // Enviarle 1 para Activar la alarma y que suene a la hora indicada (alarmaActiva). Enviarle 0 para Desactivar la alarma y que no suene a la hora indicada.
    //
    
    Time.zone(-3); // HUSO HORARIO PARA RTC INTERNO
    
    Serial1.begin(9600); // INICIO DE COMUNICACION SERIAL PARA PANTALLA

    // INICIALIZACION DE MODO DE PINES (ENTRADAS Y SALIDAS)
    pinMode(pFotoresistor,INPUT);  
    pinMode(fFotoresistor,OUTPUT);
    pinMode(pBoton, INPUT);
    //
    
    digitalWrite(fFotoresistor,HIGH); // ACTIVAR ALIMENTACION DEL LDR
    
    // ACTUALIZACIONES DE INICIO
    actualizarFechaHora();
    delay(1000);
    obtenerPronostico();
    obtenerCotizacion();
    delay(3000); // ESPERA 3 SEGUNDOS PARA NO SOBRECARGAR LA API (1 pedido por segundo)
    actualizarTemperaturaHumedad();
    //actualizarNivelLuz();
    //

    // INICIO DE TIMERS
    tFechaHora.start();
    tTemperaturaHumedad.start();
    tPronostico.start();
    tNivelLuz.start();
    tCotizacion.start();
    //

    attachInterrupt(pBoton, interrupcionBotonAlarma, FALLING); // CREACION DE LA INTERRUPCION PARA EL BOTON QUE PARA LA ALARMA
    
    Pantalla::cambiarPagina("1"); // CAMBIA LA PANTALLA AL RELOJ
}

void loop() {
    
    switch (estado) {
        case 0: // ESTADO 0
            estadoEspera();
            break;
        case 1: // ESTADO 1
            estadoAlarma();
            break;
        case 2: // ESTADO 2
            estadoDurmiendo();
            break;
    }
    
}

/* 
FUNCION EJECUTADA POR EL WEBHOOK. RECIBE LOS DATOS DEL WEBSERVICE DE PRONOSTICO
Se obtienen los datos en formato:
TemperaturaPromedio-TemperaturaMinima-TemperaturaMaxima-Estado(Lluvia-Despejado-etc)
El STRTOK se utiliza para separar los distintos datos y poder enviarlos a la pantalla con las funciones de la clase Pantalla.
*/
void actualizarPronostico(const char *event, const char *data){
    // desc - min - max - estado
    
    String str = String(data);
    char buffer[100];
    str.toCharArray(buffer, 100);
    char *token = strtok(buffer, "-");
    Pantalla::setTexto("pclima.d1t", token);
    token = strtok(NULL, "-");
    Pantalla::setTexto("pclima.d1tmi", token);
    token = strtok(NULL, "-");
    Pantalla::setTexto("pclima.d1tma", token);
    token = strtok(NULL, "-");
    Pantalla::setTexto("pclima.d1estado", token);
    Pantalla::refrescarImagen("pclima.d1"+String(token)); // PONE ENCIMA DE LAS DEMAS IMÁGENES A LA DEL CLIMA DEL DIA.
    
    token = strtok(NULL, "-");
    Pantalla::setTexto("pclima.d2t", token);
    token = strtok(NULL, "-");
    Pantalla::setTexto("pclima.d2tmi", token);
    token = strtok(NULL, "-");
    Pantalla::setTexto("pclima.d2tma", token);
    token = strtok(NULL, "-");
    Pantalla::setTexto("pclima.d2estado", token);
    Pantalla::refrescarImagen("pclima.d2"+String(token));
    
    token = strtok(NULL, "-");
    Pantalla::setTexto("pclima.d3t", token);
    token = strtok(NULL, "-");
    Pantalla::setTexto("pclima.d3tmi", token);
    token = strtok(NULL, "-");
    Pantalla::setTexto("pclima.d3tma", token);
    token = strtok(NULL, "-");
    Pantalla::setTexto("pclima.d3estado", token);
    Pantalla::refrescarImagen("pclima.d3"+String(token));
}

/* 
FUNCION EJECUTADA POR EL WEBHOOK. RECIBE LOS DATOS DEL WEBSERVICE DE COTIZACION
Se obtienen los datos en formato JSON:
{"USD_ARS": VALOR}
Siempre tiene el mismo formato asi que se corta directamente los valores con substrings y con la busqueda del punto se toman solo 2 decimales.
*/
void actualizarCotizacion(const char *event, const char *data){
    String cot = String(data);
    String cambio = cot.substring(2,5);
    String precio = cot.substring(11, cot.indexOf(".")+3);
    Pantalla::setTexto("pcambio."+String(cambio), precio);
}

// EFUNCION QUE PIDE AL WEBSERVICE PEDIR LOS DATOS DEL PRONONOSTICO
void obtenerPronostico(){
    Particle.publish("pronostico", ubicacion);
}

// EFUNCION QUE PIDE AL WEBSERVICE PEDIR LOS DATOS DE COTIZACAION. SON 3 LLAMADAS PORQUE LA VERSION GRATUITA SOLO PERMITE 1 COTIZACION POR PEDIDO.
void obtenerCotizacion(){
    Particle.publish("cotizacion", "USD_ARS");
    Particle.publish("cotizacion", "EUR_ARS");
    Particle.publish("cotizacion", "BTC_ARS");
}

void estadoEspera(){ // ESTADO 0 - VERIFICA SI LA HORA ACTUAL ES LA HORA DE ALARMA
    if (horaAlarma.equals(Time.format(Time.now(), "%H:%M"))){
        if (alarmaActiva){
            estado = 1;
            sonando = 1;
        }
        return;
    }
    delay(500);
}

void estadoAlarma(){ // ESTADO 1 - LA ALARMA SUENA Y TOCA LA CANCION.
    buzzer.siguienteNota();
}

void estadoDurmiendo(){ // ESTADO 2 - LA ALARMA DUERME HASTA QUE LA HORA ES DISTINTA A LA DE LA ALARMA (PARA QUE NO SUENE DE VUELTA).
    if (!horaAlarma.equals(Time.format(Time.now(), "%H:%M"))){
        buzzer.reiniciar();
        estado = 0;
        sonando = 0;
    }
    delay(500);
}

void interrupcionBotonAlarma(){ // RUTINA DE INTERRUPCION QUE EJECUTA AL PRESIONAR EL BOTON PARA PARAR ALARMA.
    estado = 2;
    sonando = false;
}

int setAlarma(String a){ // CAMBIA LA HORA DE ALARMA
    horaAlarma = a;
    buzzer.reiniciar();
    estado = 0;
    sonando = 0;
    Eeprom::almacenarHoraAlarma(a);
    return 1;
}

int setCancion(String c){
    if (estado == 0 || estado == 2) {
        buzzer.setCancion(c.toInt());
        Eeprom::almacenarCancion(c.toInt());
        cancionElegida = c.toInt();
    }
    return 1;
}

// FUNCION QUE ACTUALIZA LA FECHA Y LA HORA CON EL RELOJ INTERNO.
void actualizarFechaHora(){ 
    time_t timenow = Time.now();
    String aux = Time.format(timenow, "%H:%M:%S");
    Pantalla::setTexto("preloj.reloj", aux);
    aux = Time.format(timenow, "%d/%m/%Y");
    if (!aux.equals(fecha_anterior)){ // evitar mandar la fecha cada segundo
        Pantalla::setTexto("preloj.calendario",aux);
        fecha_anterior = aux;
    }
}

// FUNCION QUE ACTUALIZA EL NIVEL DE LUZ DE LA PANTALLA SEGUN EL VALOR DEL LDR
void actualizarNivelLuz(){
    if (brilloLDR){
        vFotoresistor = analogRead(pFotoresistor);
        //nivel_luz = String((int)((((float)vFotoresistor/4096)*100)/2 + 50));
        
        int aux_luz = (int)(((float)vFotoresistor/1024)*100) + 5;
        if (aux_luz > 100) {
            aux_luz = 100;
        }
        nivel_luz = String(aux_luz);
        if (!nivel_luz.equals(nivel_luz_anterior)){
            Pantalla::setBrillo(nivel_luz);
            nivel_luz_anterior = nivel_luz;
        }
    } else {
        Pantalla::setBrillo(nivel_luz);
    }
    
}

/* 
FUNCION QUE ACTUALIZA LOS VALORES DE TEMPERATURA Y HUMEDAD EN LA PANTALLA EN BASE A LOS VALORES RECIBIDOS POR EL DHT11
*/
void actualizarTemperaturaHumedad(){
    int respuesta = dht.acquireAndWait(1000);   
    switch (respuesta) {
        case DHTLIB_OK:
            temp = (int)dht.getCelsius();
            hum = (int)dht.getHumidity();
            Pantalla::setTexto("preloj.temperatura", String(temp));
            Pantalla::setTexto("preloj.humedad", String(hum));
            break;
        case DHTLIB_ERROR_CHECKSUM:
            Particle.publish(String("CHECKSUM"));
            break;
        case DHTLIB_ERROR_ISR_TIMEOUT:
            Particle.publish(String("ISR TIMEOUT"));
            break;
        case DHTLIB_ERROR_RESPONSE_TIMEOUT:
            Particle.publish(String("RESPONSE TIMEOUT"));
            break;
        case DHTLIB_ERROR_DATA_TIMEOUT:
            Particle.publish(String("DATA TIMEOUT"));
            break;
        case DHTLIB_ERROR_ACQUIRING:
            Particle.publish(String("ACQUIRING"));
            break;
        case DHTLIB_ERROR_DELTA:
            Particle.publish(String("DELTA TIME TOO SMALL"));
            break;
        case DHTLIB_ERROR_NOTSTARTED:
            Particle.publish(String("NOT STARTED"));
            break;
        default:
            Particle.publish(String("UNKNOWN"));
            break;
    }
}

int setBrillo(String valor){ // COLOCA EL BRILLO DE LA PANTALLA EN EL VALOR ELEGIDO. (0-100)
    nivel_luz = valor;
    brilloLDR = false;
    return 1;
}

int toggleBrillo(String tog){ // ACTIVA O DESACTIVA EL CONTROL DE BRILLO INTERNO
    brilloLDR = tog.equals("1");
    return 1;
}

int setUbicacion(String ub){ // CAMBIA LA UBICACION PARA OBTENER EL PRONOSTICO Y OBTIENE EL PRONOSTICO DE LA NUEVA UBICACION. RECIBE LATITUD Y LONGITUD EN FORMATO "LATITUD;LONGITUD;"
    String str = String(ub);
    char buffer[100];
    float aux;
    str.toCharArray(buffer, 100);
    char* lat = strtok(buffer, ";");
    Eeprom::almacenarLatitud(atof(lat));
    char* lon = strtok(NULL, ";");
    Eeprom::almacenarLongitud(atof(lon));
    ubicacion = String::format("{\"lat\":%s, \"lon\":%s}", lat, lon);
    obtenerPronostico();
    return 1;
}

int apagarAlarma(String a){ // APAGA LA ALARMA
    interrupcionBotonAlarma();
    return 1;
}

int toggleAlarma(String a){ // ACTIVA O DESACTIVA LA ALARMA
    alarmaActiva = a.equals("1");
    Eeprom::almacenarEstadoAlarma(alarmaActiva);
    if (alarmaActiva) Pantalla::setColorReloj(String(SKY_BLUE));
    else Pantalla::setColorReloj(String(GREY));
    return 1;
}
