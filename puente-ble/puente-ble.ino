/*
  Chispa EFI · Puente Bluetooth para Speeduino
  =============================================
  Convierte un ESP32 en un puente transparente entre Bluetooth LE (servicio
  Nordic UART) y el segundo puerto serial de Speeduino en modo "Tuner Studio".
  Chispa se conecta desde Chrome (Android o computadora) en Conexión → Bluetooth
  y ve un dispositivo llamado "Chispa-XXXX".

  Placas: ESP32 DevKit (ESP32-WROOM-32) o ESP32-C3 (SuperMini, DevKitM).
  Core de Arduino: "esp32" de Espressif, versión 2.x o 3.x. No hace falta
  instalar ninguna librería extra.

  Conexiones con ESP32 DevKit (en la C3: RX = GPIO4, TX = GPIO5):

    Mega D14 (TX3) ──[1 kΩ]──┬──── ESP32 GPIO16 (RX)   el Mega manda 5 V y el
                             └─[2 kΩ]── GND            ESP32 aguanta 3.3 V
    Mega D15 (RX3) ─────────────── ESP32 GPIO17 (TX)
    Mega 5V ────────────────────── ESP32 VIN / 5V
    Mega GND ───────────────────── ESP32 GND

  En TunerStudio, antes de usarlo:
    Accessories → Canbus/Secondary Serial IO Interface
      Enable Second Serial  = Enable
      Second Serial protocol = Tuner Studio
    Burn y reiniciar el Mega. Con este modo no se puede usar el USB y el
    Bluetooth al mismo tiempo: cerrá TunerStudio y Chispa por USB.
*/

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// ------------------------------------------------------------ configuración
#define ECU_BAUD 115200            // Speeduino abre el segundo serial a 115200

#if defined(CONFIG_IDF_TARGET_ESP32C3)
  #define PIN_ECU_RX 4
  #define PIN_ECU_TX 5
  #define PIN_LED    8             // LED de la C3 SuperMini, se prende con LOW
  #define LED_ACTIVO LOW
#else
  // ESP32 clásico. Si tu placa es WROVER (con PSRAM), GPIO16/17 están ocupados:
  // usá por ejemplo 26 (RX) y 27 (TX).
  #define PIN_ECU_RX 16
  #define PIN_ECU_TX 17
  #define PIN_LED    2             // LED azul del DevKit, se prende con HIGH
  #define LED_ACTIVO HIGH
#endif
// Si tu placa no tiene LED, poné PIN_LED en -1.

#define NUS_SERVICE "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
#define NUS_RX      "6e400002-b5a3-f393-e0a9-e50e24dcca9e"   // el celular escribe acá
#define NUS_TX      "6e400003-b5a3-f393-e0a9-e50e24dcca9e"   // el puente notifica acá

// ------------------------------------------------------------ estado
HardwareSerial EcuSerial(1);
BLEServer *server = nullptr;
BLECharacteristic *txChar = nullptr;

volatile bool conectado = false;
bool estabaConectado = false;
uint16_t bloqueBle = 20;           // bytes por notificación = MTU − 3
uint32_t ultimoMtu = 0;
uint32_t ultimoInforme = 0;
volatile uint32_t ultimoTrafico = 0;
uint32_t bytesAEcu = 0;            // solo para el informe; no hace falta que sea exacto
uint32_t bytesACelular = 0;

class ServidorCB : public BLEServerCallbacks {
  void onConnect(BLEServer *) override { conectado = true; }
  void onDisconnect(BLEServer *) override { conectado = false; }
};

// Celular → ECU: todo lo que llega por Bluetooth sale tal cual por el serial.
class RxCB : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *c) override {
    uint8_t *datos = c->getData();
    size_t n = c->getLength();
    if (datos != nullptr && n > 0) {
      EcuSerial.write(datos, n);
      bytesAEcu += n;
      ultimoTrafico = millis();
    }
  }
};

// ECU → celular, partido en paquetes del tamaño que permite la conexión.
void enviarAlCelular(uint8_t *datos, size_t n) {
  for (size_t i = 0; i < n; i += bloqueBle) {
    size_t k = (n - i < bloqueBle) ? (n - i) : bloqueBle;
    txChar->setValue(datos + i, k);
    txChar->notify();
    if (i + k < n) delay(3);       // aire para la pila BLE entre paquetes
  }
}

void actualizarLed() {
  if (PIN_LED < 0) return;
  uint32_t t = millis();
  bool prendido;
  if (!conectado) prendido = (t / 500) % 2;                  // parpadeo lento: esperando al celular
  else if (t - ultimoTrafico < 80) prendido = (t / 40) % 2;  // titila: pasan datos
  else prendido = true;                                      // fijo: conectado
  digitalWrite(PIN_LED, prendido ? LED_ACTIVO : !LED_ACTIVO);
}

void setup() {
  Serial.begin(115200);
  delay(300);
  if (PIN_LED >= 0) pinMode(PIN_LED, OUTPUT);

  EcuSerial.setRxBufferSize(1024);
  EcuSerial.begin(ECU_BAUD, SERIAL_8N1, PIN_ECU_RX, PIN_ECU_TX);

  // Nombre único con los últimos dígitos de la MAC, por si hay varios puentes cerca.
  char nombre[20];
  uint64_t mac = ESP.getEfuseMac();
  snprintf(nombre, sizeof(nombre), "Chispa-%04X", (unsigned)((mac >> 32) & 0xFFFF));

  BLEDevice::init(nombre);
  BLEDevice::setMTU(247);          // el celular puede negociar menos; se ajusta solo

  server = BLEDevice::createServer();
  server->setCallbacks(new ServidorCB());

  BLEService *svc = server->createService(NUS_SERVICE);
  txChar = svc->createCharacteristic(NUS_TX, BLECharacteristic::PROPERTY_NOTIFY);
#if !defined(CONFIG_NIMBLE_ENABLED)
  txChar->addDescriptor(new BLE2902());   // con NimBLE este descriptor se agrega solo
#endif
  BLECharacteristic *rxChar = svc->createCharacteristic(
      NUS_RX, BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR);
  rxChar->setCallbacks(new RxCB());
  svc->start();

  server->getAdvertising()->start();

  Serial.println();
  Serial.printf("Chispa · puente Bluetooth listo como \"%s\"\n", nombre);
  Serial.printf("Serial a la ECU: RX=GPIO%d TX=GPIO%d a %d baudios\n", PIN_ECU_RX, PIN_ECU_TX, ECU_BAUD);
  Serial.println("Esperando al celular…");
}

void loop() {
  // Cambios de conexión
  if (conectado && !estabaConectado) {
    estabaConectado = true;
    while (EcuSerial.available()) EcuSerial.read();   // descartar restos viejos
    bloqueBle = 20;
    ultimoMtu = 0;
    Serial.println("Celular conectado.");
  }
  if (!conectado && estabaConectado) {
    estabaConectado = false;
    delay(300);
    server->startAdvertising();
    Serial.println("Celular desconectado. Esperando conexión…");
  }

  // El MTU se negocia después de conectar: revisarlo cada medio segundo.
  if (conectado && millis() - ultimoMtu > 500) {
    ultimoMtu = millis();
    int mtu = server->getPeerMTU(server->getConnId());
    uint16_t nuevo = (uint16_t)constrain(mtu - 3, 20, 244);
    if (nuevo != bloqueBle) {
      bloqueBle = nuevo;
      Serial.printf("MTU negociado: %d (paquetes de %u bytes)\n", mtu, bloqueBle);
    }
  }

  // ECU → celular: juntar la respuesta entera antes de mandarla.
  if (EcuSerial.available()) {
    static uint8_t buf[512];
    size_t n = 0;
    uint32_t ultimoByte = micros();
    while (n < sizeof(buf)) {
      if (EcuSerial.available()) {
        buf[n++] = (uint8_t)EcuSerial.read();
        ultimoByte = micros();
      } else if (micros() - ultimoByte > 1200) {
        break;                     // ~14 bytes de silencio a 115200: terminó el paquete
      }
    }
    if (conectado) enviarAlCelular(buf, n);   // sin celular, se descarta
    bytesACelular += n;
    ultimoTrafico = millis();
  }

  // Informe por el monitor serie cada 5 s mientras hay conexión
  if (conectado && millis() - ultimoInforme > 5000) {
    ultimoInforme = millis();
    Serial.printf("Tráfico: %lu bytes al Mega · %lu bytes al celular\n",
                  (unsigned long)bytesAEcu, (unsigned long)bytesACelular);
  }

  actualizarLed();
}
