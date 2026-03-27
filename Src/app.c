#if 0
/**
 * @file app.c
 * @brief Proyecto de ejemplo que demuestra la adquisición de audio I2S desde un micrófono MEMS INMP441.
 *
 * Este ejemplo muestra cómo usar el periférico I2S del STM32 en modo DMA para adquirir datos
 * de audio de un micrófono MEMS digital INMP441. Las muestras adquiridas se pueden enviar por UART para
 * procesamiento adicional o visualización en una PC. La grabación se controla mediante el botón de usuario
 * (B1), y un LED de estado indica el estado actual de la adquisición.
 *
 * Hardware:
 *   - MCU de la serie STM32F3
 *   - Micrófono MEMS INMP441 (Interfaz I2S)
 *   - UART para salida en serie (460800 Bits/s)
 *   - Botón de usuario (B1) para iniciar/detener
 *   - LED de estado (LD2)
 *
 * Uso:
 *   - Presione el botón de usuario para iniciar o detener la adquisición de audio.
 *   - Las muestras de audio se transmiten por UART en un formato binario simple o se procesan.
 */
#endif

#include "main.h"
#include "app.h"

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

// Inclusiones de CMSIS-DSP y extracción modular
#include <arm_math.h>
#include "mfcc_features.h"
#include "clasificador_svm.h"

/******** Definiciones ******** */
#define SAMPLES_PER_HOP 256U /**< @brief 32 ms a 8 kHz */
#define HOPS_PER_FRAME 8U    /**< @brief Duración total del frame de 256 ms a 8 kHz */
#define FULL_BUFFER_SIZE (SAMPLES_PER_HOP * HOPS_PER_FRAME) /**< @brief 256 ms de audio a 8 kHz */

#define NOISE_ALPHA 0.01f /**< @brief Coeficiente para la actualización del nivel de ruido (suavizado exponencial) */

/**
 * @brief Estructura para el envío de datos en flotante por UART.
 */
typedef union {
    float32_t f32;
    uint8_t b[4];
} float_packet_t;

// Indica si el micrófono está grabando actualmente
static volatile bool is_recording = false;

// Buffers para muestras estéreo I2S (2 palabras para los canales izquierdo y derecho)
static uint8_t i2s_stereo_samples[SAMPLES_PER_HOP * 2 * 4]; /**< @brief Dos canales, 4 bytes por muestra. */
static uint8_t sample_buff[SAMPLES_PER_HOP * 2 * 4];        /**< @brief Buffer para un hop de muestras mono (32 ms). */
static float32_t full_buff[FULL_BUFFER_SIZE];               /**< @brief 256 ms de muestras mono a 8 kHz. */
static float32_t hop[SAMPLES_PER_HOP];                      /**< @brief Buffer del hop actual. */

// Matriz MFCC para el frame [8 hops]
float32_t mfcc_matrix[HOPS_PER_FRAME][MFCC_COEFFS_NUM]; 

// Promedio y varianza de los coeficientes MFCC del frame actual.
#define FEATURE_VECTOR_SIZE (2 * MFCC_COEFFS_NUM) /**< @brief Media + varianza para cada coeficiente */
static float32_t feature_vector[FEATURE_VECTOR_SIZE]; 

// Variables globales
float32_t g_noise_floor =  0.01f; /**< @brief Valor inicial del ruido de fondo (RMS) */
bool g_signal_detected  = false;  /**< @brief Indica si se ha detectado una señal por encima del umbral de ruido */
bool g_dma_data_ready   = false;  /**< @brief Indica si la transferencia DMA se ha completado */

// Manejadores externos para I2S y UART (definidos en otra parte)
extern I2S_HandleTypeDef hi2s2;
extern UART_HandleTypeDef huart2;

// Prototipos de funciones internas
static void mic_start(void);
static void mic_stop(void);
static inline float32_t i2s_sample_to_float32(uint8_t* sample);

/**
 * @brief  Actualiza el LED de estado para indicar el estado de la grabación.
 * @param  recording Verdadero si está grabando, falso en caso contrario.
 */
static inline void update_status_led(bool recording)
{
    if (recording)
    {
        // Enciende el LED de estado (ej. LD2)
        HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_SET);
    }
    else
    {
        // Apaga el LED de estado
        HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);
    }
}

/**
 * @brief  Bucle principal de la aplicación.
 *         Maneja la inicialización y procesa la señal de audio capturada.
 */
void app_run(void)
{
    // Inicialización del estado
    is_recording = false;
    update_status_led(is_recording);
    memset(i2s_stereo_samples, 0, sizeof(i2s_stereo_samples));
    uint16_t hop_index = 0;

    // Inicializar la extracción de características MFCC y el clasificador SVM
    mfcc_features_init();
    clasificador_svm_init();

    while(1)
    {
        if(g_signal_detected)
        {
            if(g_dma_data_ready && hop_index < 8) // Solo procesar si hay datos DMA listos y no hemos llenado el buffer completo
            {
                memcpy(full_buff + (hop_index * SAMPLES_PER_HOP), hop, sizeof(hop));
                g_dma_data_ready = false; 

                // Calcular MFCCs del hop actual
                mfcc_features_compute(hop, mfcc_matrix[hop_index]);
                hop_index++;

            } else if (hop_index == 8)
            {
                mfcc_features_mean_and_std(&mfcc_matrix[0][0], HOPS_PER_FRAME, MFCC_COEFFS_NUM, feature_vector);

                // Clasificar la muestra con el modelo SVM
                int32_t svm_result = -1;
                clasificador_svm_predict(feature_vector, FEATURE_VECTOR_SIZE, &svm_result);

                hop_index = 0;
                g_signal_detected = false;

                // Enviar resultado por UART
                const char* msg = "Vocal: ";
                if(svm_result != -1)
                {
                    HAL_UART_Transmit(&huart2, (uint8_t*)msg, strlen(msg), 100U);
                    HAL_UART_Transmit(&huart2, (uint8_t*)&svm_result, 1, 100U);
                    HAL_UART_Transmit(&huart2, (uint8_t*)"\r\n", 2, 100U);
                }
            }
        }
    }
}

/**
 * @brief  Inicia la adquisición del micrófono utilizando I2S DMA.
 *         Actualiza el estado y notifica mediante UART.
 */
static void mic_start(void)
{
    if (!is_recording)
    {
        /* Inicia la recepción de DMA de I2S (2 palabras, 24 bits cada una)
           El argumento del tamaño es uint16_t*, pero la API HAL lo espera así para formatos de 24 o 32 bits.
        */
        if (HAL_I2S_Receive_DMA(&hi2s2, (uint16_t*)i2s_stereo_samples, 2 * SAMPLES_PER_HOP) == HAL_OK)
        {
           is_recording = true;
           update_status_led(is_recording);

           // Notificar que la adquisición ha comenzado
           const char* msg = "Microfono encendido\r\n";
           HAL_UART_Transmit(&huart2, (uint8_t*)msg, strlen(msg), 100U);
        } 
        else 
        {
           // Notificar error
           const char* msg = "Mic acquisition: START ERROR\r\n";
           HAL_UART_Transmit(&huart2, (uint8_t*)msg, strlen(msg), 100U);
        }
    } 
}

/**
 * @brief  Detiene la adquisición del micrófono y de DMA.
 *         Actualiza el estado y notifica mediante UART.
 */
static void mic_stop(void)
{
    if (is_recording)
    {
        // Detiene el DMA de I2S
        HAL_I2S_DMAStop(&hi2s2);
        is_recording = false;
        update_status_led(is_recording);

        // Notificar que la adquisición ha finalizado
        const char* msg = "Microfono apagado\r\n";
        HAL_UART_Transmit(&huart2, (uint8_t*)msg, strlen(msg), 100U);
    }
}

/**
 * @brief  Llamada de retorno (callback) para la detección de línea EXTI (presión del botón).
 *         Cambia el estado de la grabación cuando se presiona el botón del usuario (B1).
 * @param  GPIO_Pin Especifica el pin conectado a la línea EXTI.
 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == B1_Pin)
    {
        // Cambia el estado de grabación al presionar el botón
        if (is_recording)
        {
            mic_stop();
        }
        else
        {
            mic_start();
        }
    }
}

/**
 * @brief  Llamada de retorno de finalización de recepción DMA I2S.
 *         Se invoca cuando se completa la recepción de un bloque de datos I2S.
 *         Envía las muestras recibidas al buffer por medio de un procesamiento.
 * @param  hi2s Manejador de la estructura I2S.
 */
void HAL_I2S_RxCpltCallback(I2S_HandleTypeDef *hi2s)
{
    // Solo se procesa si es nuestra instancia I2S
    if(hi2s->Instance != hi2s2.Instance)
    {
        return; 
    }

    // Solo se procesa si se está grabando
    if(is_recording == false)
    {
        return; 
    }

    memcpy(sample_buff, i2s_stereo_samples, sizeof(sample_buff));

    g_dma_data_ready = true; // Indica que los datos DMA están listos para ser procesados
    
    float_packet_t result;

    for (uint32_t i = 0; i < SAMPLES_PER_HOP; i++)
    {
        // Convierte la muestra del canal izquierdo (los primeros 4 bytes de cada par estéreo)
        uint8_t* sample_ptr = &sample_buff[i * 8]; // 8 bytes por muestra estéreo (4 izq, 4 der)
        hop[i] = i2s_sample_to_float32(sample_ptr);
    }

    arm_rms_f32((float32_t *)hop, SAMPLES_PER_HOP, &result.f32);

    if(result.f32 > 7.5 * g_noise_floor)
    {
        g_signal_detected = true;
    } else
    {
        // Actualizar nivel de ruido.
        g_noise_floor = (1.0f - NOISE_ALPHA) * g_noise_floor +
                    NOISE_ALPHA * result.f32;
    }
}

/**
 * @brief  Convierte un arreglo de bytes en una muestra de audio en coma flotante.
 * @param  sample Puntero a la muestra raw (8 bytes).
 * @return Muestra en punto flotante 32-bits normalizada al rango [-1.0, 1.0].
 */
static inline float32_t i2s_sample_to_float32(uint8_t* sample)
{
    int32_t reord_sample = (int32_t)( (sample[1] << 16 ) |
                                      (sample[0] << 8  ) |
                                      (sample[3]       )
							        );

    if (reord_sample & 0x00800000)   // Si el bit de signo de 24 bits está establecido
    {
        reord_sample |= 0xFF000000;  // Extender signo a 32 bits
    }

    // Convertir la muestra de 24 bits a un valor flotante en el rango [-1.0, 1.0]
    return (float32_t) (reord_sample / 8388608.0f); // Se divide por 2^23 para normalizar
}
