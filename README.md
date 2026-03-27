# Sistema de Detección de Vocales

Este proyecto implementa un clasificador SVM para la detección de vocales utilizando un microcontrolador STM32F3. El sistema adquiere audio mediante un micrófono MEMS INMP441, extrae características MFCC y clasifica la vocal utilizando Máquinas de Vectores de Soporte (SVM).

## Características Principales

- **Adquisición de Audio**: Interfaz I2S con micrófono MEMS INMP441 a 8 kHz.
- **Procesamiento de Señal**:
  - Extracción de 13 coeficientes MFCC por frame.
  - Hop de 32 ms (256 muestras).
  - Ventanas de 256 ms (8 hops).
  - Cálculo de medias y desviaciones estándar de los coeficientes MFCC.
- **Clasificación**:
  - Implementación de múltiples clasificadores SVM "uno contra el resto" usando la librería CMSIS-DSP.
- **Interfaz de Usuario**:
  - Botón de usuario (B1) para iniciar/detener la grabación.
  - LED de estado que indica el estado de la grabación.
  - Comunicación UART para depuración y reporte de resultados.

## Estructura del Proyecto

```
wake_up_word_svm/
├── Inc/                      # Cabeceras de la aplicación
│   ├── app.h                 # Cabecera principal de la aplicación
│   ├── clasificador_svm.h    # Definiciones del clasificador SVM
│   ├── mfcc_features.h       # Definiciones de extracción de MFCC
│   └── ...                   # Otras cabeceras del sistema
├── Src/                      # Código fuente
│   ├── app.c                 # Lógica principal de la aplicación
│   ├── clasificador_svm.c    # Implementación de los clasificadores SVM
│   ├── mfcc_features.c       # Implementación de extracción de MFCC
│   └── ...                   # Código de inicialización y HAL
├── STM32CubeIDE/             # Configuración del IDE
├── Drivers/                  # Controladores HAL y CMSIS
└── README.md                 # Documentación del proyecto
```

## Requisitos de Hardware

- Microcontrolador STM32F3xx
- Micrófono MEMS INMP441
- LED de estado (LD2)
- Botón de usuario (B1)

<p align="center">
<img src="https://drive.google.com/uc?export=view&id=1k-mYVQVk2T5NjttuTqAW95a4pcD9Cpar" width="500">
<p>

## Configuración

### Frecuencia de Muestreo
El sistema está configurado para una frecuencia de muestreo de **8 kHz**.

### Parámetros de MFCC
- **Tamaño del hop**: 256 muestras (32 ms)
- **Número de hops por frame**: 8
- **Duración total del frame**: 256 ms
- **Número de coeficientes MFCC**: 13

### Parámetros de Detección
- **Umbral de detección de señal**: 0.01 (RMS)
- **Filtro de ruido**: Suavizado exponencial con alpha = 0.01

## Uso

1. **Compilar y flashear** el firmware en el microcontrolador usando STM32CubeIDE.
2. **Presionar el botón de usuario (B1)** para iniciar la adquisición de audio. Se mostrará el mensaje "Iniciando grabacion" o "Grabacion detenida" por UART cada que se presione el botón.
3. El **LED de estado** se iluminará mientras el sistema está grabando.
4. El sistema procesará el audio en bloques de 256 ms y clasificará las vocales detectadas.
5. Los resultados se mostrarán por **UART** en el siguiente formato:
   ```
   Vocal: X
   ```
   Donde `X` es el número de la vocal detectada.

### Monitorización Serial (UART)
Para visualizar los escaneos y resultados emitidos por el microcontrolador, debes configurar correctamente tu terminal serial. El sistema opera a una velocidad muy alta para transferir los vectores flotantes a tiempo. El puerto debe estar ajustado concretamente a un **Baudrate de 460800 bps** (8 bits de datos, sin paridad, 1 bit de parada).

#### En Windows
Puedes usar los siguientes programas con interfaz gráfica comúnmente recomendados:
- **PuTTY**: En *Connection type* selecciona `Serial`. En *Serial line* especifica tu puerto (ej. `COM3`) y cambia el *Speed* (baudrate) a `460800`.
- **Tera Term** o **RealTerm**: Asegúrate de seleccionar el número de puerto COM correspondiente e ir a la configuración Serial Port para asignar la misma tasa de audios (460800).

#### En Linux
En distribuciones robustas como Ubuntu/Debian, el puerto de tu placa frecuentemente se instanciará sobre `/dev/ttyACM0` (o `ttyUSB0`). Puedes valerte de utilidades de terminal como `screen` o `picocom`.

Ejemplo con `screen`:
```bash
sudo screen /dev/ttyACM0 460800
```
*(Para salir de la interfaz screen presiona `Ctrl+A` seguido de `K`)*

Opcionalmente, usando el robusto y ligero `picocom`:
```bash
sudo picocom -b 460800 /dev/ttyACM0
```

## Notas de Implementación

- La función `app_run()` es el bucle principal de la aplicación.
- Las interrupciones de GPIO y I2S manejan la adquisición de datos y el control del sistema.
- Se utiliza la biblioteca CMSIS-DSP para el procesamiento eficiente de señales.

## Mediciones

| Función | Tiempo de ejecución |
|---------|-------------|
| `mfcc_features_compute` | 1400 µs |
| `clasificador_svm_predict` | 130 - 160 µs |