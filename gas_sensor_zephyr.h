/*
 * Zephyr RTOS Integration Example
 * 
 * This file demonstrates how to integrate the gas sensor API
 * with Zephyr RTOS for use on embedded systems.
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/logging/log.h>

#include "gas_sensor.h"

LOG_MODULE_REGISTER(gas_sensor, CONFIG_GAS_SENSOR_LOG_LEVEL);

/* ============================================================================
 * Zephyr UART Integration
 * ============================================================================ */

/*
 * For Zephyr, the serial communication can be handled by:
 * 1. Using the UART device driver directly (recommended)
 * 2. Wrapping the gas_sensor API with Zephyr uart drivers
 * 3. Using a UART ISR-driven ring buffer approach
 */

/* Device tree compatible gas sensor configuration example:
 * 
 * In your device tree (.dts) file:
 * 
 * / {
 *     aliases {
 *         gas-sensor-uart = &uart2;
 *     };
 * };
 * 
 * In prj.conf:
 * 
 * CONFIG_UART_ASYNC_API=y
 * CONFIG_SERIAL=y
 * CONFIG_GAS_SENSOR_LOG_LEVEL=4
 */

/* ============================================================================
 * Zephyr-Based Gas Sensor Handler
 * ============================================================================ */

typedef struct {
    gas_sensor_handle_t sensor;
    gas_sensor_slow_data_t slow_data;
    gas_sensor_waveform_t waveform;
    gas_sensor_status_t status;
    struct k_mutex lock;
} zephyr_gas_sensor_t;

/*
 * Initialize gas sensor for Zephyr
 * 
 * This function sets up the sensor with proper Zephyr integration
 */
int zephyr_gas_sensor_init(zephyr_gas_sensor_t *sensor,
                          const char *uart_device,
                          gas_sensor_callback_t callback)
{
    if (!sensor || !uart_device) {
        LOG_ERR("Invalid parameters");
        return -EINVAL;
    }
    
    /* Initialize mutex for thread safety */
    k_mutex_init(&sensor->lock);
    
    /* Initialize sensor with callback */
    int result = gas_sensor_init(uart_device, callback, &sensor->sensor);
    if (result != GAS_SENSOR_OK) {
        LOG_ERR("Failed to initialize sensor: %s",
                gas_sensor_strerror(result));
        return result;
    }
    
    /* Initialize data structures */
    gas_sensor_init_slow_data(&sensor->slow_data);
    memset(&sensor->waveform, 0, sizeof(sensor->waveform));
    memset(&sensor->status, 0, sizeof(sensor->status));
    
    LOG_INF("Gas sensor initialized on %s", uart_device);
    
    return GAS_SENSOR_OK;
}

/*
 * Thread-safe frame reading
 */
int zephyr_gas_sensor_read(zephyr_gas_sensor_t *sensor)
{
    if (!sensor) {
        return -EINVAL;
    }
    
    /* Acquire mutex */
    k_mutex_lock(&sensor->lock, K_FOREVER);
    
    /* Read frame */
    int result = gas_sensor_read_frame(sensor->sensor,
                                      &sensor->slow_data,
                                      &sensor->waveform,
                                      &sensor->status);
    
    /* Release mutex */
    k_mutex_unlock(&sensor->lock);
    
    return result;
}

/*
 * Get current sensor data (thread-safe)
 */
int zephyr_gas_sensor_get_data(zephyr_gas_sensor_t *sensor,
                              gas_sensor_slow_data_t **slow_data,
                              gas_sensor_waveform_t **waveform,
                              gas_sensor_status_t **status)
{
    if (!sensor || !slow_data || !waveform || !status) {
        return -EINVAL;
    }
    
    /* Acquire mutex */
    k_mutex_lock(&sensor->lock, K_FOREVER);
    
    *slow_data = &sensor->slow_data;
    *waveform = &sensor->waveform;
    *status = &sensor->status;
    
    /* NOTE: Caller must release mutex when done */
    
    return GAS_SENSOR_OK;
}

void zephyr_gas_sensor_release(zephyr_gas_sensor_t *sensor)
{
    if (sensor) {
        k_mutex_unlock(&sensor->lock);
    }
}

/*
 * Cleanup
 */
int zephyr_gas_sensor_deinit(zephyr_gas_sensor_t *sensor)
{
    if (!sensor) {
        return -EINVAL;
    }
    
    return gas_sensor_close(sensor->sensor);
}

/* ============================================================================
 * Zephyr Worker Thread Example
 * ============================================================================ */

/*
 * Example: Dedicated worker thread for sensor reading
 * 
 * This allows the main application to read sensor data without blocking
 */

K_THREAD_DEFINE(gas_sensor_thread, 1024, 
                zephyr_gas_sensor_worker, NULL, NULL, NULL,
                5, 0, 0);

void zephyr_gas_sensor_worker(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);
    
    LOG_INF("Gas sensor worker thread started");
    
    /* Initialize sensor - implementation depends on board configuration */
    // zephyr_gas_sensor_t sensor;
    // zephyr_gas_sensor_init(&sensor, "/dev/ttyUSB0", NULL);
    
    /* Main loop */
    while (1) {
        // int result = zephyr_gas_sensor_read(&sensor);
        // if (result == GAS_SENSOR_OK) {
        //     LOG_INF("CO2: %.2f%%, O2: %.2f%%",
        //             sensor.waveform.co2, sensor.waveform.o2);
        // }
        
        k_msleep(50);  /* Match frame timing (20Hz) */
    }
}

/* ============================================================================
 * Work Queue Example (Alternative)
 * ============================================================================ */

/*
 * Using Zephyr work queue instead of dedicated thread
 * Useful for power-constrained systems
 */

K_WORK_DELAYABLE_DEFINE(gas_sensor_work, zephyr_gas_sensor_work_handler);

void zephyr_gas_sensor_work_handler(struct k_work *work)
{
    ARG_UNUSED(work);
    
    /* Read sensor frame */
    // int result = zephyr_gas_sensor_read(&g_sensor);
    // if (result == GAS_SENSOR_OK) {
    //     /* Process data */
    // }
    
    /* Re-schedule for next frame (50ms) */
    k_work_schedule(&gas_sensor_work, K_MSEC(50));
}

void zephyr_gas_sensor_start_work_queue(void)
{
    k_work_schedule(&gas_sensor_work, K_MSEC(50));
}

/* ============================================================================
 * Zephyr Callback Example
 * ============================================================================ */

int zephyr_gas_sensor_callback(gas_sensor_slow_data_t *slow_data,
                              gas_sensor_waveform_t *waveform,
                              gas_sensor_status_t *status)
{
    /* This function is called from ISR/high priority context */
    /* Keep it short and non-blocking */
    
    /* Example: Check for critical alarms */
    if (status->sensor_error) {
        LOG_ERR("Sensor error!");
        /* Could trigger emergency action */
    }
    
    if (status->apnea) {
        LOG_WRN("Apnea detected!");
    }
    
    return GAS_SENSOR_OK;
}

/* ============================================================================
 * Sample Application Integration
 * ============================================================================ */

/*
 * Example main application structure
 */

static zephyr_gas_sensor_t g_sensor;

int app_init_gas_sensor(void)
{
    /* Get UART device from device tree */
    const struct device *uart_dev = DEVICE_DT_GET_ANY(/*uart compatible*/);
    
    if (!device_is_ready(uart_dev)) {
        LOG_ERR("UART device not ready");
        return -ENODEV;
    }
    
    /* Initialize sensor */
    return zephyr_gas_sensor_init(&g_sensor, 
                                 uart_dev,
                                 zephyr_gas_sensor_callback);
}

void app_read_sensor_data(void)
{
    int result = zephyr_gas_sensor_read(&g_sensor);
    
    if (result == GAS_SENSOR_OK) {
        /* Get data safely */
        gas_sensor_slow_data_t *slow;
        gas_sensor_waveform_t *wave;
        gas_sensor_status_t *stat;
        
        zephyr_gas_sensor_get_data(&g_sensor, &slow, &wave, &stat);
        
        LOG_INF("FrameID=%d CO2=%.2f%% O2=%.2f%% Breath=%d",
                slow->last_frame_id, wave->co2, wave->o2,
                stat->breath_detected);
        
        zephyr_gas_sensor_release(&g_sensor);
    } else {
        LOG_WRN("Read error: %s", gas_sensor_strerror(result));
    }
}

/* ============================================================================
 * prj.conf Configuration Reference
 * ============================================================================ */

/*
CONFIG_SERIAL=y
CONFIG_UART=y
CONFIG_UART_INTERRUPT_DRIVEN=y  # Or use async API
CONFIG_CONSOLE=y
CONFIG_LOG=y
CONFIG_LOG_DEFAULT_LEVEL=3
CONFIG_GAS_SENSOR_LOG_LEVEL=4

# For memory-constrained devices:
CONFIG_MINIMAL_LIBC=y
CONFIG_HEAP_MEM_POOL_SIZE=8192

# For thread-safe access:
CONFIG_KERNEL_MEM_POOL=y
CONFIG_HEAP_MEM_POOL_SIZE=16384
*/

/* ============================================================================
 * CMakeLists.txt for Zephyr Integration
 * ============================================================================ */

/*
cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(gas_sensor_app)

target_sources(app PRIVATE
    src/main.c
    src/gas_sensor.c
)

target_include_directories(app PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
*/
