/*
        Código para protocolo de cantos real vs sintetico o de perfomance

  Graba en un intervalo de fecha/hora definido por el usuario haciendo uso del RTC
  Permite grabar y reproducir usando la placa de Comunicación Inalámbrica
  Usa el RTC para Iniciar o Detener la Grabación

  Programa para Protocolo Real vs Sintetico
  Cambia canto en cada ciclo (Archivo_a_Reproducir)
  Colocar los archivos a reproducir numerados consecutivamente (1...N)
  1->R; 2->S; 3->R; 4->S; .....

  Duerme por intervalos de aproximadamente kDescanso*8 segundos

  21 de Octubre de 2024
*/

#include <TMRpcm.h>
#include "RTClib.h"
#include "LowPower.h"
#include "SoftwareSerial.h"
#include "DFRobotDFPlayerMini.h"

#include <SD.h>
//========================= CONFIGURACIONES de SOFTWARE ========================

//constexpr uint8_t Fecha_Inicio_grabacion[] = {1, 1};
//constexpr uint8_t Fecha_Final_grabacion[] = {31, 12};

constexpr uint8_t Inicio_grabacion[]   = {5, 30};  // Hora inicial de grabación
constexpr uint8_t Final_grabacion[]    = {11, 30}; // Hora final de grabación

constexpr uint8_t Inicio_grabacion_2[] = {17, 30}; // Inicio 2° intervalo
constexpr uint8_t Final_grabacion_2[]  = {19, 30}; // Final 2° intervalo

constexpr uint8_t  DuracionArchivo  = 2;     // min por archivo de grabación
constexpr uint16_t FreqMuestreo     = 22000; // Hz

constexpr uint8_t ComienzaReproduccion = 0;  // (no se usa en esta versión)
constexpr uint8_t SilencioCantos       = 20; // segundos entre INICIOS de cantos
constexpr uint8_t CantidadCantos       = 1;  // repeticiones de cada archivo

constexpr uint8_t DuracionProtocolo    = 6;  // minutos de protocolo
constexpr uint8_t Descanso             = 21; // minutos de descanso entre protocolos

constexpr uint8_t Volumen        = 20;  // 0–30
constexpr uint8_t NumMaxCantosSD = 6;  // cantidad máxima de archivos en la carpeta
//========================= CONFIGURACIONES de HARDWARE ========================
const uint8_t RstPin     = 3;
const uint8_t LED_Work   = 4;
const uint8_t LED_Error  = 5;

const uint8_t Arduino_SD = 8;
const uint8_t ESP_SD     = 9;

#define SD_ChipSelectPin 10
#define MIC A0

SoftwareSerial mySoftwareSerial(6, 7);
//========================= DECLARACIONES DE OBJETOS ===========================
RTC_DS3231 rtc;
TMRpcm audio;
DFRobotDFPlayerMini myDFPlayer;
//========================= VARIABLES GLOBALES ================================
char filename[13];

bool Dormir          = false;
bool chequeoIntervalo = true;

bool PrimeraVez = false;  // Inicio de protocolo
bool Siguiente  = false;  // Ir al próximo fichero de grabación

uint32_t tiempoFichero;
constexpr uint32_t SaltoFichero = DuracionArchivo * 60000UL;

constexpr uint32_t inicioReproduccion = ComienzaReproduccion * 60000UL;
bool Reproducir = false;
bool Ciclo      = false;

constexpr uint32_t Silencio = SilencioCantos * 1000UL;
uint32_t TiempoUltimoCanto = 0;
uint8_t ContadorCantos     = 0;
uint8_t Archivo_a_Reproducir = 1;  // archivos numerados 1..NumMaxCantosSD

uint32_t TiempoInicio = 0;
constexpr uint32_t TiempoTotal = DuracionProtocolo * 60000UL;
constexpr uint16_t kDescanso   = (Descanso * 60) / 8.3;
//========================= FUNCIONES GENERALES ================================
void dateTime(uint16_t* date, uint16_t* time) {
  DateTime now = rtc.now();
  *date = FAT_DATE(now.year(), now.month(),  now.day());
  *time = FAT_TIME(now.hour(), now.minute(), now.second());
}
//======================
void getFileName() {
  DateTime now = rtc.now();
  sprintf(filename, "%02d%02d%02d%02d.wav",
          now.day(), now.hour(), now.minute(), now.second());
}
//======================
void LedError() {
  digitalWrite(LED_Error, LOW);
  digitalWrite(LED_Work, LOW);
  for (int j = 1; j < 6; j++) {
    digitalWrite(LED_Error, HIGH); delay(100);
    digitalWrite(LED_Error, LOW);  delay(500);
  }
  pinMode(RstPin, OUTPUT);
  digitalWrite(RstPin, LOW);
}
//======================
void IniciarSistema() {
  if (!rtc.begin()) LedError();

  if (rtc.lostPower()) {
    //rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    rtc.adjust(DateTime(2222, 12, 13, 8, 0, 0));
  }

  audio.CSPin = SD_ChipSelectPin;
  if (!SD.begin(SD_ChipSelectPin)) LedError();

  mySoftwareSerial.begin(9600);
  delay(3000);
  if (!myDFPlayer.begin(mySoftwareSerial)) LedError();
  myDFPlayer.volume(Volumen);
  delay(1000);
  myDFPlayer.playFolder(15, 1); // Carpeta 15, archivo 1
  delay(10000);
}
//======================
// Función para grabar
void grabarAudio() {
  getFileName();
  SdFile::dateTimeCallback(dateTime);
  audio.startRecording(filename, FreqMuestreo, MIC);
  digitalWrite(LED_Work, HIGH);
  tiempoFichero = millis();
}
//======================
// Función para detener la grabación
void detenerGrabacion() {
  digitalWrite(LED_Work, LOW);
  audio.stopRecording(filename);
}
//======================
// Función para reproducir un archivo de audio
// *** MODIFICADA para recorrer 1,2,3... dentro del protocolo ***
void reproducirCanto() {
  TiempoUltimoCanto = millis();
  myDFPlayer.volume(Volumen);
  myDFPlayer.playFolder(15, Archivo_a_Reproducir);

  // Contamos cuántas veces sonó ESTE archivo
  ContadorCantos++;

  // Si ya sonó CantidadCantos veces, pasamos al siguiente archivo
  if (ContadorCantos >= CantidadCantos) {
    ContadorCantos = 0;
    Archivo_a_Reproducir++;
    if (Archivo_a_Reproducir > NumMaxCantosSD) {
      Archivo_a_Reproducir = 1;  // volver al primero
    }
  }
  // OJO: NO tocamos Ciclo ni Reproducir acá.
}
//======================
// Guardar log de inicio de protocolo
void registrarReproduccion(DateTime now, uint8_t archivo) {
  SdFile::dateTimeCallback(dateTime);
  File logFile = SD.open("LOG.txt", FILE_WRITE);
  if (logFile) {
    logFile.print("- Inicio: ");
    logFile.print(now.year());
    logFile.print("-");
    logFile.print(now.month());
    logFile.print("-");
    logFile.print(now.day());
    logFile.print(" ");
    logFile.print(now.hour());
    logFile.print(":");
    logFile.print(now.minute());
    logFile.print(":");
    logFile.print(now.second());
    logFile.print(" - Archivo inicial: ");
    logFile.println(archivo);
    logFile.close();
  }
}
//======================
// Verificar si la hora está dentro de los intervalos
bool estaEnIntervalo(DateTime now) {
  uint16_t tiempo_actual = now.hour() * 60 + now.minute();
  uint16_t tiempo_inicio_grabacion   = Inicio_grabacion[0]   * 60 + Inicio_grabacion[1];
  uint16_t tiempo_final_grabacion    = Final_grabacion[0]    * 60 + Final_grabacion[1];
  uint16_t tiempo_inicio_grabacion_2 = Inicio_grabacion_2[0] * 60 + Inicio_grabacion_2[1];
  uint16_t tiempo_final_grabacion_2  = Final_grabacion_2[0]  * 60 + Final_grabacion_2[1];

  return (tiempo_actual >= tiempo_inicio_grabacion   && tiempo_actual <= tiempo_final_grabacion) ||
         (tiempo_actual >= tiempo_inicio_grabacion_2 && tiempo_actual <= tiempo_final_grabacion_2);
}

//====================== SET UP DEL MODULO =====================================
void setup() {

  // *** Para la placa de Sistema Unificado comentar esta parte *** //
  pinMode(Arduino_SD, OUTPUT);
  digitalWrite(Arduino_SD, HIGH);
  delay(1000);
  pinMode(ESP_SD, OUTPUT);
  digitalWrite(ESP_SD, LOW);
  delay(1000);
  //*** Fin de parte a comentar **** //

  pinMode(LED_Work, OUTPUT);
  pinMode(LED_Error, OUTPUT);
  pinMode(MIC, INPUT);

  IniciarSistema();
}
//====================== LOOP GRABACION / REPRODUCCION =========================
void loop() {

  if (chequeoIntervalo) {
    chequeoIntervalo = false;
    DateTime now = rtc.now();
    if (estaEnIntervalo(now)) {
      Dormir     = false;
      PrimeraVez = true;
    } else {
      Dormir = true;
    }
  }

  if (PrimeraVez) {
    PrimeraVez = false;

    // *** REINICIAR SECUENCIA AL COMIENZO DEL PROTOCOLO ***
    Archivo_a_Reproducir = 1;
    ContadorCantos = 0;

    DateTime now = rtc.now();
    registrarReproduccion(now, Archivo_a_Reproducir);
    delay(2000);
    grabarAudio();
    TiempoInicio = millis();
    Ciclo = true;
    // Reproducir seguirá en false hasta terminar el primer archivo de 2 min
  }

  // Cierre de archivo de 2 min
  if (millis() - tiempoFichero > SaltoFichero) {
    detenerGrabacion();
    Siguiente  = true;
    Reproducir = true;  // a partir de acá se permite reproducir cantos
  }

  if (Siguiente) {
    Siguiente = false;
    grabarAudio();
  }

  // Reproducción de cantos dentro del protocolo
  if (Ciclo && Reproducir && (millis() - TiempoUltimoCanto > Silencio)) {
    reproducirCanto();
  }

  // Fin de protocolo o salida de intervalo
  if ((millis() - TiempoInicio > TiempoTotal) || Dormir) {
    Dormir = false;
    detenerGrabacion();
    delay(2000);
    Siguiente  = false;
    Reproducir = false;
    Ciclo      = false;

    // Descanso
    for (uint8_t k = 1; k <= kDescanso; k++) {
      LowPower.idle(SLEEP_8S, ADC_OFF, TIMER2_OFF, TIMER1_OFF, TIMER0_OFF,
                    SPI_OFF, USART0_OFF, TWI_OFF);
    }
    if (rtc.lostPower()) {
      rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
      // rtc.adjust(DateTime(1111, 11, 11, 11, 11, 0));
    }
    chequeoIntervalo = true;
  }
}

