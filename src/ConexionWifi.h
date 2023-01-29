#include <WiFiManager.h>
#include "ConexionLcd.h"
#include <Ticker.h>

#define ledWifi 4 // 2
Ticker ticker;    // Objeto creado tipo ticker

// variables para la conexion de la red
String redWifi;
IPAddress ipaddress;
// Propiedades PWM
int frecuencia = 5000; // Frecuencia estandar
int canal = 1;         // CAnal pwm
int resolucion = 8;    // la resolucion de 8 bits de 0-255
int estado;

void parpadear()
{
  estado = !estado;
  if (estado == 1)
  {
    ledcWrite(canal, 15);
  }
  else
  {
    ledcWrite(canal, 0);
  }
}

void ConectarWifi()
{
  pinMode(ledWifi, OUTPUT);
  // Configuramos la funcionalidad del PWM
  ledcSetup(canal, frecuencia, resolucion);
  // asociamos el canal al GPIO
  ledcAttachPin(ledWifi, canal);

  WiFiManager wm;
  ticker.attach(0.4, parpadear); // Inicia la ejecucion cada 0.4ms se llama la funcion en un bucle

  // wm.resetSettings(); //Reseteamos la configuracion
  bool res = wm.autoConnect("Esp32_CAM"); // Creamos Ap y portal cautivo

  if (!res)
  {
    Serial.println("Error al conectarse a la red Wifi");
    ESP.restart();
  }
  else
  {
    Serial.println("Ya estas conectado :)");
    ticker.detach(); // detenemos la ejecucion
  }
  // APagamos el led
  ledcWrite(canal, 0);
  redWifi = WiFi.SSID();      // Nombre de la red wifi
  ipaddress = WiFi.localIP(); // Ip corresponiente

  // Mostramos el mensaje por consola
  Serial.println("");
  Serial.println("********************************************");
  Serial.print("Conectado a la red WiFi: ");
  Serial.println(redWifi);
  Serial.print("IP: ");
  Serial.println(ipaddress);
  Serial.println("*********************************************");

  EncenderLCD(redWifi, ipaddress); // Mostramos el msj de la ip en la lcd
}