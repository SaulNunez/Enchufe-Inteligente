# Enchufe Inteligente

Programa para el ESP8266 para crear un enchufe inteligente controlado por Alexa.

Funciones:
* Controla dos enchufes por Alexa.
* Conecta el enchufe a tu red a base de una pagina web donde puedes meter las credenciales de tu WiFi.
  * Crea una red WiFi donde al entrar a la dirección dada, se abre para meter las credenciales. Algo así como la configuración del Chromecast.
  * Guarda la información de tu red directamente a memoria interna del ESP8266 mediante LittleFS.
* Botones para apagar/encender el enchufe sin usar voz
  * Actualiza el estado a Alexa
