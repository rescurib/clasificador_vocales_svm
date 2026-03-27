/**
 * @file mfcc_features.c
 * @brief Implementación de las funciones de extracción de MFCC.
 */

#include "mfcc_features.h"
#include "mfcc_config.h"

#define FFT_SIZE 256U /**< @brief Tamaño de la Transformada Rápida de Fourier (FFT) para MFCC */

// Instancia global para almacenar el contexto del MFCC
static arm_mfcc_instance_f32 mfcc_ctx;

// Buffer interno utilizado durante el cálculo MFCC por CMSIS-DSP
static float32_t mfcc_complex_buff[2 * FFT_SIZE];

/**
 * @brief Inicializa el extractor de características MFCC utilizando los parámetros precomputados.
 */
void mfcc_features_init(void)
{
    arm_mfcc_init_f32(&mfcc_ctx,
                      FFT_SIZE, 
                      NB_MFCC_NB_FILTER_CONFIG_8K_F32, 
                      MFCC_COEFFS_NUM, 
                      mfcc_dct_coefs_config1_f32, 
                      mfcc_filter_pos_config_8k_f32, 
                      mfcc_filter_len_config_8k_f32,
                      mfcc_filter_coefs_config_8k_f32,
                      mfcc_window_coefs_config3_f32);
}

/**
 * @brief  Ejecuta la extracción de MFCC a partir de la muestra de audio.
 * @param  audio_buffer Puntero al bloque de la señal de audio.
 * @param  out_buffer   Puntero al inicio del buffer que contendrá los coeficientes calculados.
 */
void mfcc_features_compute(float32_t *audio_buffer, float32_t *out_buffer)
{
    arm_mfcc_f32(&mfcc_ctx, audio_buffer, out_buffer, mfcc_complex_buff);
}

/**
 * @brief  Computa la media y la desviación estándar para condensar varios frames MFCC en un único vector.
 * @param  mfcc_mat       Puntero lineal a la matriz de resultados MFCC de tamaño [num_hops][num_coeffs].
 * @param  num_hops       El total de iteraciones / subdivisiones (hops) capturadas.
 * @param  num_coeffs     La cantidad de coeficientes que se extraen por frame.
 * @param  feature_vector Salida final con medias seguidas de desviaciones estándar.
 */
void mfcc_features_mean_and_std(float32_t *mfcc_mat, uint32_t num_hops, uint32_t num_coeffs, float32_t *feature_vector)
{
    float32_t temp_col_buf[num_hops];
    
    for( uint16_t i = 0; i < num_coeffs; i++)
    {
        for( uint16_t j = 0; j < num_hops; j++)
        {
            // Extraer la columna i-ésima de la matriz MFCC
            temp_col_buf[j] = *(mfcc_mat + (j * num_coeffs) + i); 
        }
        arm_mean_f32(temp_col_buf, num_hops, &feature_vector[i]);
        arm_std_f32(temp_col_buf, num_hops, &feature_vector[i + num_coeffs]);
    }  
}
