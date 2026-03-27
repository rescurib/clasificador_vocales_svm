# Entrenamiento del Modelo SVM

Este directorio contiene los conjuntos de datos recopilados y los scripts de Python necesarios para analizar, entrenar y formatear las Máquinas de Vectores de Soporte (SVM) que el firmware utiliza en la tarjeta STM32.

## Obtención de Muestras (Dataset)
Para recolectar tus propias muestras, el firmware original debe estar modificado o ajustado en modo de "volcado de características" (feature dump). En este modo, transmitirá crudamente los arreglos finales de parámetros MFCC (usualmente 26 datos: 13 medias y 13 desviaciones estándar de un frame) por puerto UART hacia la terminal.

1. He dejado listo este modo en un _tag_ específico de Git y puedes hacer "checkout" de la siguiente manera para obtenerlo:
   ```bash
   git checkout mfcc_sampler
   ```
2. Compila ese firmware temporal, flashea tu placa, y conéctala a la PC.
3. Utiliza el script `serial_to_features.py` (ubicado en el _directorio raíz_ del proyecto) para capturar la ráfaga de datos flotantes entrantes por UART y aglomerarlos en archivos `.npy`. Por ejemplo:
   ```bash
   python serial_to_features.py --output Training/mfcc_a.npy
   ```
   *Deberás repetir este proceso y renombrar el parámetro `--output` para cada clase nueva que desees grabar.*

## Entrenando el Modelo (`train_svm_mfcc.py`)
Una vez tengas los archivos recolectados en este directorio (por ejemplo: `mfcc_a.npy`, `mfcc_e.npy`, `mfcc_i.npy`, `mfcc_o.npy`, `mfcc_u.npy`), estás listo para lanzar el script principal de entrenamiento:

```bash
python train_svm_mfcc.py
```

### ¿Cómo funciona internamente `train_svm_mfcc.py`?
1. **Carga Automática**: Busca cualquier archivo nominado como `mfcc_*.npy` dentro de este directorio. Los concatena verticalmente y les asigna su propia etiqueta de texto leyendo dinámicamente el nombre del archivo.
2. **Lineal SVM bajo la regla OVR**: Como la librería CMSIS-DSP es eficiente evaluando modelos lineales binarios, Python divide el problema Multiclase usando `OneVsRestClassifier` instanciando `LinearSVC` desde _Scikit-Learn_. Generará internamente $N$ clasificadores independientes.
3. **Métricas del Modelo**: Evalúa y divide el conjunto de datos (80% entrenamiento, 20% prueba), e imprime por consola un reporte detallado de precisión y recuperación (Accuracy & Recall).
4. **Renderización de Cabecera C**: Extrae matemáticamente la configuración geométrica de los modelos entrenados (.coef_ e .intercept_) y construye/formatea arreglos constantes en lenguaje C. Por último, guarda e imprime el archivo **`svm_params.h`**, que es exactamente el mismo que importa `clasificador_svm.c` en el firmware.

## Visualización (`plot_mfcc_pca.py`)
Para constatar la calidad de nuestro dataset o audios recuperados, puedes ejecutar este script auxiliar de visualización:
```bash
python plot_mfcc_pca.py
```
El script leerá los archivos en el mismo formato, los estandarizará, y aplicará Análisis de Componentes Principales (PCA) para comprimir la dimensionalidad geométrica y mostrarte una figura en 2D. Con esto, confirmarás visualmente si las vocales capturadas logran agruparse (ser separables) en el espectro MFCC.
