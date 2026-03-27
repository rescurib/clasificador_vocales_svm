/**
 * @file clasificador_svm.h
 * @brief Definiciones y prototipos del módulo de clasificación Máquinas de Vectores de Soporte (SVM).
 */

#ifndef CLASIFICADOR_SVM_H
#define CLASIFICADOR_SVM_H

#include <stdint.h>
#include <arm_math.h>

/**
 * @brief Inicializa los contextos de todos los clasificadores SVM "uno vs el resto".
 */
void clasificador_svm_init(void);

/**
 * @brief  Predice a qué clase pertenece un vector de características dado.
 * @param  feature_vector      Vector de características ya compilado (medias y varianzas).
 * @param  feature_vector_size La cantidad de elementos en el vector proporcionado.
 * @param  label               Puntero a entero donde reescribir la etiqueta encontrada, o -1 en caso de desconocida.
 */
void clasificador_svm_predict(float32_t *feature_vector, uint32_t feature_vector_size, int32_t *label);

#endif /* CLASIFICADOR_SVM_H */