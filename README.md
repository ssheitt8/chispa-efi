# Chispa EFI: prototipo de ECU programable + app de afinación

"Chispa" es un nombre provisional. Cambialo cuando quieras.

- **App:** https://ssheitt8.github.io/chispa-efi/ (con Chrome o Edge, en computadora o Android, se instala como app).
- **Guía del banco de pruebas:** https://ssheitt8.github.io/chispa-efi/banco/

## Qué hay en este repositorio

| Archivo | Para qué sirve |
|---|---|
| `app.html` | Código fuente de la app (HTML + CSS + JS, sin dependencias). Es lo que se publica como Artifact. |
| `banco.html` | Código fuente de la guía del banco de pruebas: compras, diagrama, pines, ajustes de TunerStudio, puente Bluetooth y pruebas. |
| `build.sh` | Arma el sitio en `docs/`: le pone a cada página su encabezado completo, el manifiesto y el service worker, y cambia los enlaces de Artifact por enlaces internos. |
| `docs/` | El sitio que publica GitHub Pages. Se genera con `sh build.sh`; no lo edites a mano. |
| `pwa/` | Manifiesto, service worker, `pwa.js` (instalación) e íconos. `iconos.ps1` regenera los PNG. |
| `servir.ps1` | Servidor local mínimo en PowerShell para usar Chispa sin internet en `http://localhost:8765`. |
| `puente-ble/puente-ble.ino` | Firmware del puente Bluetooth para ESP32 (core 2.x o 3.x, sin librerías extra). Ver abajo. |

## Cómo publicar un cambio

1. Editá `app.html` o `banco.html`.
2. Corré `sh build.sh` (Git Bash).
3. Hacé commit y push a `main`. GitHub Pages publica `docs/` en uno o dos minutos.
4. Si cambiás archivos de `pwa/`, subí la versión de `CACHE` en `pwa/sw.js` (`chispa-v2`, `chispa-v3`…) para que los celulares descarten lo viejo.

## Asistente de mapa base (pestaña Motor)

Con los datos del motor, los inyectores y el combustible, el asistente genera un punto de partida.

**Lo que escribe en la ECU**
- Inyector base (`reqFuel`, página 1, offset 24).
- Tiempo muerto (`injOpen`, offset 27).
- Estequiométrico (offset 50).
- Las tablas VE (página 2), de avance (página 3) y de AFR objetivo (página 5), con sus ejes.

Antes de escribir guarda una copia del mapa anterior, y el botón **Volver al mapa anterior** lo repone. Después hay que **Quemar** y reiniciar la ECU, porque Speeduino toma `reqFuel` e `injOpen` recién al arrancar.

**Cálculos**
- **Inyector base:** volumen de un cilindro × densidad del aire (1.1843 g/L, a 100 kPa y 21 °C) ÷ estequiométrico ÷ densidad del combustible (0.72 g/cc, igual que TunerStudio) ÷ caudal en cc/ms.
  - El caudal se corrige por presión: × √(presión de trabajo ÷ presión de calibración).
  - Si hay menos inyectores que cilindros, el resultado se multiplica por cilindros ÷ inyectores.
  - Es el pulso del ciclo completo. Speeduino lo divide por 2 cuando la inyección no es secuencial (`fuel_calcs.cpp`).
- **Tabla VE:** forma genérica según la preparación (de calle, leve o competición).
  - Pico en 58 %, 70 % u 80 % del régimen máximo.
  - Suma un margen de +8 % a plena carga.
  - Con "Incorporate AFR" apagado (es el valor de fábrica, en página 1, offset 70, bit 7), la tabla se divide por el lambda objetivo para que el enriquecimiento quede en la VE.
- **AFR objetivo:** lambda 1.00 hasta 60 kPa, 0.90 a 90 kPa, 0.87 a 100 kPa y 0.78 con 1 bar de turbo.
- **Avance:**
  - Curva de carga baja que va de 12–16° en ralentí a 36–38° en crucero.
  - Curva de plena carga de 26–30°, que se ajusta por octanaje (91: −2°, 98: +1.5°) y combustible (E85: +4°).
  - Con turbo se resta 1° cada 10 kPa.
- **Controles:**
  - Ciclo útil a régimen máximo y plena carga.
  - Pulso útil en ralentí.
  - Potencia que aguantan los inyectores (BSFC 0.5 para gasolina y 0.7 para E85).
  - Límite de 25.5 ms de Speeduino.
  - Rango del sensor MAP.
  - Que coincidan los cilindros con los de la ECU y que la ECU use MAP como carga.

La ECU virtual ahora usa `reqFuel`, `injOpen` y la tabla de AFR objetivo de su página 1 y su página 5, igual que Speeduino. Su motor "real" tiene 1600 cc, 4 cilindros e inyectores de 315 cc/min a 3 bar. Con esos datos el asistente da 8.5 ms, el mismo valor de fábrica.

## Puente Bluetooth (ESP32)

- Pasa el **segundo serial de Speeduino** a Bluetooth LE con el servicio Nordic UART. En el Mega es Serial3: TX3 = D14 y RX3 = D15.
- En TunerStudio hay que activarlo en Accessories → Canbus/Secondary Serial IO Interface:
  - Enable Second Serial = Enable
  - Second Serial protocol = Tuner Studio
  - Después, Burn y reiniciar el Mega.
- Con ese protocolo, Speeduino atiende el mismo protocolo que por USB, pero no deja usar USB y Bluetooth al mismo tiempo.
- Cableado:
  - D14 → 1 kΩ → RX del ESP32, con 2 kΩ de RX a GND.
  - D15 ← TX del ESP32.
  - 5V → VIN y GND → GND.
  - Pines del ESP32 DevKit: RX = GPIO16, TX = GPIO17. En el ESP32-C3: RX = GPIO4, TX = GPIO5.
- El puente se anuncia como `Chispa-XXXX` y Chispa filtra la lista por ese nombre. Las notificaciones salen en bloques de MTU − 3 bytes (de 20 a 244), con el MTU negociado.
- **No está compilado ni probado en hardware.** Se escribió contra las cabeceras de la librería BLE de arduino-esp32 en las ramas `release/v2.x` y `master` (3.x).
- Web Bluetooth necesita https o localhost. Dentro de la vista de Artifact y desde `http://192.168…` está bloqueado. Para el celular hace falta hosting https o la app nativa (en iPhone, solo la app nativa).

En Chispa, **Conexión → Diagnóstico en vivo** muestra:
- Sincronía (byte 32, bit 7), pérdidas de sincronía (byte 3) y el contador de segundos (byte 0). Si el contador vuelve a 0 fuera del desborde 255 → 0, se cuenta como un reinicio de la ECU.
- **Verificar tablas**, que compara el CRC32 de las páginas 2, 3 y 5 (comando `d`) con la copia que tiene Chispa.

## Cómo usarla

- **Sin hardware:** abrí la app y usá la **ECU virtual**. Es un motor 1.6 L de 4 cilindros simulado que responde con el protocolo binario real de Speeduino. La app usa exactamente el mismo código para la ECU virtual y para una ECU real.
- **Con una ECU Speeduino real (USB):**
  1. Abrí https://ssheitt8.github.io/chispa-efi/ en Chrome o Edge de computadora.
     Sin internet: corré `powershell -NoProfile -ExecutionPolicy Bypass -File servir.ps1` y abrí `http://localhost:8765`.
  2. En Conexión, tocá USB y elegí el puerto.
  Web Serial solo funciona en localhost o https, no dentro de la vista de Artifact.
- **Bluetooth:** necesitás el puente ESP32 de `puente-ble/` o un HM-10 en el segundo serial de la ECU. Funciona con Chrome en Android o en computadora, desde la dirección https. En iPhone no hay Web Bluetooth, así que hace falta una app nativa.

## Qué hace la app hoy

- Tablero en vivo con RPM, AFR contra objetivo (pobre o rica), MAP, TPS, avance, VE, ancho de pulso, temperaturas, batería y un gráfico de los últimos 30 s.
- Tabla VE (página 2) y tabla de avance (página 3) en 2D y 3D.
  - Punto en vivo con "Seguir punto".
  - Selección por arrastre, ±1 y ±2 %, Fijar, Interpolar (respeta el espaciado de los ejes), Suavizar y Deshacer.
- Las celdas se escriben en la RAM de la ECU al instante (comando `M`). **Quemar** las guarda en la EEPROM o flash (comando `b`).
- Autoajuste VE, solo con la ECU virtual: corrige la tabla con el AFR medido cuando el motor está caliente y estable.
- Registro de datos a 10 Hz, exportable a CSV.

## Protocolo implementado (Speeduino, formato `msEnvelope_1.0`)

Cada trama es `[largo uint16 big-endian][payload][CRC32 big-endian del payload]`. La respuesta empieza con un código: `0x00` OK, `0x04` quemado OK, `0x80` o más es error.

| Comando | Payload | Respuesta |
|---|---|---|
| Firma | `'Q'` | `0x00` + texto, por ejemplo `speeduino 202501` |
| Leer página | `'p', 0, página, offset LE16, largo LE16` | `0x00` + bytes |
| Escribir página | `'M', 0, página, offset LE16, largo LE16, datos…` | `0x00` |
| Quemar página | `'b', 0, página` | `0x04` |
| Datos en vivo | `'r', 0, 0x30, offset LE16, largo LE16` | `0x00` + bytes |

### Página 1, configuración

| Offset | Dato |
|---|---|
| 24 | reqFuel ×10 |
| 26, bits 4–6 | Carga de encendido |
| 36, bits 4–7 | Cilindros |
| 37, bits 0–2 | Algoritmo: 0 MAP, 1 TPS, 2 IMAP/EMAP |
| 50 | Estequiométrico ×10 |

### Páginas 2 y 3, tablas

- Valores 16×16 en los bytes 0–255. La fila es la carga y el índice es `y*16 + x`.
- Bines de RPM en los bytes 256–271, con valor ÷100.
- Bines de carga en los bytes 272–287: kPa ÷2, o % TPS ×2.
- En la página 3 el avance se guarda con +40.

### Datos en vivo, offsets

| Offset | Dato |
|---|---|
| 4–5 | MAP |
| 6 | IAT +40 |
| 7 | CLT +40 |
| 9 | Batería ×10 |
| 10 | AFR ×10 |
| 13 | WUE % |
| 14–15 | RPM |
| 21 | AFR objetivo ×10 |
| 24 | Avance, int8 |
| 25 | TPS ×2 |
| 76–77 | Ancho de pulso en µs |
| 102 | VE actual |

Fuente: `comms.cpp`, `logger.cpp` y `reference/speeduino.ini` del repositorio speeduino/speeduino (release 202501.7).

## Hoja de ruta propuesta

### Fase 0: lo que ya está
App prototipo con una ECU virtual que habla el protocolo real.

### Fase 1: banco de pruebas (unos US$100–150)
- Arduino Mega 2560 con una placa Speeduino v0.4, o una placa compatible armada. Otra opción es una placa basada en Teensy 4.1.
- Ardu-Stim: un Arduino Uno que simula la rueda fónica del cigüeñal, para que la ECU "vea" un motor girando.
- LEDs o lámparas de 12 V como carga de inyectores y bobinas, un potenciómetro como TPS y termistores NTC.
- Configuración inicial con TunerStudio. Después conectar Chispa por USB y verificar que lee y escribe lo mismo.

### Fase 2: hardware propio, tamaño "mini"
| Bloque | Componentes |
|---|---|
| MCU | Teensy 4.1 o Mega 2560 (lo más probado con Speeduino), o STM32F407 en PCB propia (más barato, pero revisá cuán madura está la versión STM32 del firmware). |
| Inyectores | MOSFET low-side de nivel lógico, o drivers protegidos tipo VNLD5090, con diodo de rueda libre. |
| Encendido | Salidas de 5 V para bobinas con ignitor, o IGBT (ISL9V3040) para bobinas sin ignitor. |
| Cigüeñal | Acondicionador VR MAX9926, o entrada Hall con pull-up y filtro RC. |
| Sensores | MAP integrado (MPXH6400A de 4 bar, o MPX4250AP de 2.5 bar). CLT e IAT como NTC con pull-up de 2.49 kΩ. TPS de 0–5 V. |
| Alimentación | Protección contra polaridad inversa, TVS para picos de carga, buck automotriz a 5 V y referencia de 5 V para sensores con fusible reseteable. |
| Salidas auxiliares | Relé de bomba, relé de ventilador y tacómetro. |
| Conectividad | USB-C (USB nativo del MCU) y ESP32-C3 como puente BLE para el celular. |
| Conector y carcasa | Micro-Fit 3.0, o conectores automotrices sellados (Superseal o Deutsch DT). Carcasa sellada. |

### Fase 3: app de verdad
- Pasar este prototipo a app nativa con Capacitor o Flutter más un plugin BLE (así anda en iPhone).
- Agregar:
  - Tabla de AFR objetivo (página 5) y correcciones.
  - Asistente de mapa base: cilindrada e inyectores dan el reqFuel.
  - Entrada de wideband.
  - Registro en SD y actualizaciones de firmware.

### Fase 4: producto
- Arnés, manual, mapas base por motor, garantía y soporte.
- Como referencia, la ST Mini se vende a US$340.

## Licencias (no es asesoría legal)
- **Firmware Speeduino: GPL-2.0.**
  - Podés vender hardware con ese firmware.
  - Si distribuís un firmware modificado, tenés que publicar el código fuente.
  - Una app propia que solo habla por el protocolo serial es un programa aparte.
- **rusEFI: GPL-3.0 con términos extra.** Es para vehículos fuera de calle y no cumple normas de emisiones.
- Usá un nombre y una marca propios. No copies el diseño ni el branding de otra empresa.

## Seguridad
- Probá todo en banco antes de ir al motor.
- Una tabla pobre a plena carga puede romper un motor.
- Revisá fugas y presión de combustible, y que la bomba corte con el motor apagado o al perder sincronía.
- En muchos países, cambiar la gestión del motor de un auto de calle tiene implicancias legales y de emisiones.
