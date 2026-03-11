#if 0
/**
 * @file serial_mic_recorder.c
 * @brief Example project demonstrating I2S audio acquisition from an INMP441 MEMS microphone.
 *
 * This example shows how to use the STM32 I2S peripheral in DMA mode to acquire audio data
 * from an INMP441 digital MEMS microphone. The acquired samples are sent over UART for
 * further processing or visualization on a host PC. Recording is controlled by a user button
 * (B1), and a status LED indicates the current acquisition state.
 *
 * Hardware:
 *   - STM32F3 series MCU
 *   - INMP441 MEMS microphone (I2S interface)
 *   - UART for serial output (460800 Bits/s)
 *   - User button (B1) for start/stop
 *   - Status LED (LD2)
 *
 * Usage:
 *   - Press the user button to start or stop audio acquisition.
 *   - Audio samples are streamed over UART in a simple binary format.
 */
#endif

#include "main.h"
#include "serial_mic_recorder.h"

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

// CMSIS-DSP includes
#include <arm_math.h>

/******** Definiciones ******** */
#define SAMPLES_PER_HOP 256U // 32 ms at 8 kHz

typedef union {
    float32_t f32;
    uint8_t b[4];
} float_packet_t;

// Indicates if the microphone is currently recording
static volatile bool is_recording = false;

// Buffer for I2S stereo samples (2 words for left and right channels)
static uint8_t i2s_stereo_samples[SAMPLES_PER_HOP * 2 * 4]; // Dos canales, 4 bytes por muestra.
static uint8_t sample_buff[SAMPLES_PER_HOP * 2 * 4]; // Buffer para un hop de muestras mono (32 ms)
static uint8_t hop_buff[SAMPLES_PER_HOP * 4]; // Buffer para un hop de muestras mono (32 ms)

// External handles for I2S and UART peripherals (defined elsewhere)
extern I2S_HandleTypeDef hi2s2;
extern UART_HandleTypeDef huart2;

// Internal function prototypes
static void mic_start(void);
static void mic_stop(void);
static inline float32_t i2s_sample_to_float32(uint8_t* sample);

/**
 * @brief  Update the status LED to indicate recording state.
 * @param  recording: true if recording, false otherwise
 */
static inline void update_status_led(bool recording)
{
    if (recording)
    {
        // Turn ON status LED (e.g., LD2)
        HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_SET);
    }
    else
    {
        // Turn OFF status LED
        HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);
    }
}

/**
 * @brief  Main loop for the serial microphone recorder.
 *         Handles initialization and waits for user input (button interrupt).
 */
void serial_recorder_loop(void)
{
    // Initialize state
    is_recording = false;
    update_status_led(is_recording);
    memset(i2s_stereo_samples, 0, sizeof(i2s_stereo_samples));

    while(1)
    {
        // Main loop does nothing; start/stop is interrupt-driven
        HAL_Delay(1);
    }
}

/**
 * @brief  Start microphone acquisition using I2S DMA.
 *         Updates state and notifies via UART.
 */
static void mic_start(void)
{
    if (!is_recording)
    {
        /* Start I2S DMA reception (2 words, 24 bits each)
         I know, this size argument is confusing since uint16_t* is used, 
         but the HAL API expects it this way when 24 or 32 bit data formats are used. 
        */
        if (HAL_I2S_Receive_DMA(&hi2s2, (uint16_t*)i2s_stereo_samples, 2 * SAMPLES_PER_HOP) == HAL_OK)
        {
           is_recording = true;
           update_status_led(is_recording);

           // Notify host that acquisition has started
           const char* msg = "Mic acquisition: START\r\n";
           HAL_UART_Transmit(&huart2, (uint8_t*)msg, strlen(msg), 100U);
        } 
        else 
        {
           // Notify host of error
           const char* msg = "Mic acquisition: START ERROR\r\n";
           HAL_UART_Transmit(&huart2, (uint8_t*)msg, strlen(msg), 100U);
        }
    } 
}

/**
 * @brief  Stop microphone acquisition and DMA.
 *         Updates state and notifies via UART.
 */
static void mic_stop(void)
{
    if (is_recording)
    {
        // Stop I2S DMA
        HAL_I2S_DMAStop(&hi2s2);
        is_recording = false;
        update_status_led(is_recording);

        // Notify host that acquisition has stopped
        const char* msg = "Mic acquisition: STOP\r\n";
        HAL_UART_Transmit(&huart2, (uint8_t*)msg, strlen(msg), 100U);
    }
}

/**
 * @brief  EXTI line detection callback (button press).
 *         Toggles recording state on user button (B1) press.
 * @param  GPIO_Pin: Specifies the pins connected EXTI line
 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == B1_Pin)
    {
        // Toggle recording state on button press
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

/*
HAL_I2S_RxHalfCpltCallback(I2S_HandleTypeDef *hi2s)
{
    // This callback is called when half of the DMA buffer is filled.
    // You can use this to process the first half of the samples while the second half is being filled.
}
*/

/**
 * @brief  I2S DMA receive complete callback.
 *         Called when a block of I2S data is received.
 *         Sends the received samples over UART.
 * @param  hi2s: I2S handle
 */
void HAL_I2S_RxCpltCallback(I2S_HandleTypeDef *hi2s)
{
    // Only process if this is our I2S instance
    if(hi2s->Instance != hi2s2.Instance)
    {
        return; // Not our I2S instance
    }

    // Only process if currently recording
    if(is_recording == false)
    {
        return; // Not recording, ignore
    }


    memcpy(sample_buff, i2s_stereo_samples, sizeof(sample_buff));
    
    static volatile float32_t hop[SAMPLES_PER_HOP];
    float_packet_t result;

    for (uint32_t i = 0; i < SAMPLES_PER_HOP; i++)
    {
        // Convert left channel sample (first 4 bytes of each stereo pair)
        uint8_t* sample_ptr = &sample_buff[i * 8]; // 8 bytes per stereo sample (4 for left, 4 for right)
        hop[i] = i2s_sample_to_float32(sample_ptr);
    }

    arm_rms_f32((float32_t *)hop, SAMPLES_PER_HOP, &result.f32);
    
    HAL_UART_Transmit(&huart2, result.b, sizeof(result.b), 10U);
    HAL_UART_Transmit(&huart2, (uint8_t*)"\n", 1U, 10U); // Newline for framing

    /*
    // Prepare buffer to send left channel data (low byte, middle byte, high byte, newline)
    uint8_t send_buffer[5];

    send_buffer[0] =  left_sample.b[0];
    send_buffer[1] =  left_sample.b[1];
    send_buffer[2] =  left_sample.b[2];
    send_buffer[3] =  left_sample.b[3];
    send_buffer[4] = '\n'; // Newline for framing

    // Transmit audio sample over UART (don't do this here in your actual project!)
    HAL_UART_Transmit(&huart2, send_buffer, sizeof(send_buffer), 10U);
    */
}


static inline float32_t i2s_sample_to_float32(uint8_t* sample)
{
    int32_t reord_sample = (int32_t)( (sample[1] << 16 ) |
                                      (sample[0] << 8  ) |
                                      (sample[3]       )
							        );

    if (reord_sample & 0x00800000)   // if 24-bit sign bit is set
    {
        reord_sample |= 0xFF000000;  // sign-extend to 32 bits
    }

    // Convert 24-bit signed sample to float in range [-1.0, 1.0]
    return (float32_t) (reord_sample / 8388608.0f); // Divide by 2^23 to normalize
}


