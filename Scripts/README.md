# Scripts

Directorio que contiene códigos de asistencia o utilidades en Python para el proyecto (proveídos por ARM)

## Generación de Parámetros MFCC (`mfccdata.py`)

El script `mfccdata.py` es una utilidad provista originalmente por la biblioteca CMSIS-DSP de ARM. En este proyecto se utilizó para precomputar y construir los arreglos estáticos de C que alimentan la inicialización del hardware matemático del bloque MFCC (particularmente para la función `arm_mfcc_init_f32`).

### ¿Qué arreglos de inicialización genera?
El script es capaz de diseñar los bancos de filtros paramétricos traduciendo las configuraciones físicas a las siguientes estructuras:
1. **Coeficientes de Ventana (Window Coefficients)**: Genera arreglos usando ventanas de Hamming o Hanning ajustadas a la longitud del frame de audio del proyecto (por defecto 256 muestras o 32 ms a 8 kHz).
2. **Bancos de Filtros Mel (Mel Filterbank)**: Computa la matriz de pesos espaciales convirtiendo las dimensiones frecuenciales iniciales y finales a la "Escala Mel". Transforma esto en tres arreglos: posiciones iniciales (`filtPos`), longitudes (`filtLen`), y los coeficientes empaquetados del filtro (`filters`).
3. **Matriz DCT (Discrete Cosine Transform)**: Produce la matriz ortogonal del coseno discreto requerida para decorar y reducir los logaritmos de la energía Mel en la etapa final de los coeficientes (ej. `mfcc_dct_coefs`).

### ¿Cómo se usó el script?
1. **Definición de Configuración**: Internamente se asignan las propiedades del sistema digital deseado: frecuencia de muestro (`fs=8000`), cantidad de coeficientes a obtener (`dctOutputs=13`), longitud de FFT (`FFTSize=256`), y el formato numérico (`f32`).
2. **Procesamiento Matemático**: Utiliza `numpy` y `scipy` para procesar la geometría de las curvas y los cruces y cuantizar los resultados al tipo de dato C equivalente (`float32_t`, `q31_t`, `q15_t`).
3. **Renderizado de Archivos C**: Para finalizar, a través del motor de plantillas **Jinja2** (leyendo plantillas base o inyectando los datos directamente), incrusta todas estas variables numéricas en nuevos archivos `#include` y de código fuente (`.c` y `.h`).
4. **Acoplamiento al firmware**: Los arreglos resultantes (ej. `mfcc_window_coefs_config3_f32`, `mfcc_filter_coefs_config_8k_f32`, etc.) se importaron al proyecto (dentro de `mfcc_config.h` o variables similares del firmware) y se llamaron durante la rutina `mfcc_features_init()`.

### Dependencias requeridas
En caso de que en el futuro requieras volver a generar dichos vectores usando nuevas especificaciones (ej. para 16 kHz), necesitarás correr el script en un entorno de Python con los siguientes paquetes:
```bash
pip install numpy scipy jinja2
```
