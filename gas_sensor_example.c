/*
 * Gas Sensor API - Example Usage
 * 
 * This example demonstrates how to use the gas sensor API
 * to communicate with an anesthetic gas analyzer sensor.
 */

#include "gas_sensor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

static volatile int running = 1;

/* Signal handler to gracefully exit */
static void signal_handler(int signum)
{
    running = 0;
}

/*
 * Example callback function
 * 
 * This is called whenever a complete frame is received and parsed.
 * It demonstrates how to access the different data structures.
 */
int sensor_callback(gas_sensor_slow_data_t *slow_data,
                   gas_sensor_waveform_t *waveform,
                   gas_sensor_status_t *status)
{
    static int frame_count = 0;
    frame_count++;
    
    printf("\n=== Frame %d (FrameID: %d) ===\n", frame_count, slow_data->last_frame_id);
    
    /* Display status flags */
    printf("Status: ");
    if (status->breath_detected)
        printf("[BREATH] ");
    if (status->apnea)
        printf("[APNEA] ");
    if (status->o2_low)
        printf("[O2_LOW] ");
    if (status->o2_replace)
        printf("[REPLACE_O2] ");
    if (status->check_adapter)
        printf("[CHECK_ADAPTER] ");
    if (status->accuracy_out_of_range)
        printf("[ACCURACY_OUT_OF_RANGE] ");
    if (status->sensor_error)
        printf("[SENSOR_ERROR] ");
    if (status->o2_calibration_required)
        printf("[O2_CALIB_REQUIRED] ");
    if (!status->breath_detected && !status->apnea && !status->o2_low &&
        !status->o2_replace && !status->check_adapter && !status->accuracy_out_of_range &&
        !status->sensor_error && !status->o2_calibration_required)
        printf("OK");
    printf("\n");
    
    /* Display waveform data */
    printf("Waveform (Fast): CO2=%.2f%%, N2O=%.2f%%, AA1=%.2f%%, AA2=%.2f%%, O2=%.2f%%\n",
           waveform->co2, waveform->n2o, waveform->aa1, waveform->aa2, waveform->o2);
    
    /* Display slow data based on frame ID */
    switch (slow_data->last_frame_id) {
        case 0x00:
            printf("Inspiration Values:\n");
            printf("  CO2=%.0f%%, N2O=%.0f%%, AA1=%.0f%%, AA2=%.0f%%, O2=%.0f%%\n",
                   slow_data->insp_vals.co2, slow_data->insp_vals.n2o,
                   slow_data->insp_vals.aa1, slow_data->insp_vals.aa2,
                   slow_data->insp_vals.o2);
            break;
            
        case 0x01:
            printf("Expiration Values:\n");
            printf("  CO2=%.0f%%, N2O=%.0f%%, AA1=%.0f%%, AA2=%.0f%%, O2=%.0f%%\n",
                   slow_data->exp_vals.co2, slow_data->exp_vals.n2o,
                   slow_data->exp_vals.aa1, slow_data->exp_vals.aa2,
                   slow_data->exp_vals.o2);
            break;
            
        case 0x02:
            printf("Momentary Values:\n");
            printf("  CO2=%.0f%%, N2O=%.0f%%, AA1=%.0f%%, AA2=%.0f%%, O2=%.0f%%\n",
                   slow_data->mom_vals.co2, slow_data->mom_vals.n2o,
                   slow_data->mom_vals.aa1, slow_data->mom_vals.aa2,
                   slow_data->mom_vals.o2);
            break;
            
        case 0x03:
            printf("General Values:\n");
            if (slow_data->gen_vals.resp_rate != GAS_SENSOR_NO_DATA)
                printf("  Resp Rate: %u bpm\n", slow_data->gen_vals.resp_rate);
            if (slow_data->gen_vals.time_since_breath != GAS_SENSOR_NO_DATA)
                printf("  Time since breath: %u s\n", slow_data->gen_vals.time_since_breath);
            if (slow_data->gen_vals.primary_agent != GAS_AGENT_NONE)
                printf("  Primary Agent: %d\n", slow_data->gen_vals.primary_agent);
            if (slow_data->gen_vals.secondary_agent != GAS_AGENT_NONE)
                printf("  Secondary Agent: %d\n", slow_data->gen_vals.secondary_agent);
            if (slow_data->gen_vals.atm_pressure >= 0)
                printf("  Atm Pressure: %.1f kPa\n", slow_data->gen_vals.atm_pressure);
            break;
            
        case 0x04:
            printf("Sensor Registers:\n");
            printf("  Mode: %d, ", slow_data->sensor_regs.mode);
            printf("SW_ERR=%d, HW_ERR=%d, MOTOR_FAIL=%d, UNCAL=%d\n",
                   slow_data->sensor_regs.error.sw_error,
                   slow_data->sensor_regs.error.hw_error,
                   slow_data->sensor_regs.error.motor_fail,
                   slow_data->sensor_regs.error.uncalibrated);
            break;
            
        case 0x05:
            printf("Configuration Data:\n");
            printf("  Fitted: O2=%d, CO2=%d, N2O=%d, HAL=%d, ENF=%d, ISO=%d, SEV=%d, DES=%d\n",
                   slow_data->config_data.o2_fitted,
                   slow_data->config_data.co2_fitted,
                   slow_data->config_data.n2o_fitted,
                   slow_data->config_data.halothane_fitted,
                   slow_data->config_data.enflurane_fitted,
                   slow_data->config_data.isoflurane_fitted,
                   slow_data->config_data.sevoflurane_fitted,
                   slow_data->config_data.desflurane_fitted);
            printf("  HW Rev: 0x%04X, SW Rev: 0x%04X, S/N: 0x%04X\n",
                   slow_data->config_data.hw_revision,
                   slow_data->config_data.sw_revision,
                   slow_data->service_data.serial_number);
            break;
            
        case 0x06:
            printf("Service Data:\n");
            printf("  S/N: 0x%04X, Zero_disabled=%d, Zero_in_progress=%d\n",
                   slow_data->service_data.serial_number,
                   slow_data->service_data.status.zero_disabled,
                   slow_data->service_data.status.zero_in_progress);
            break;
            
        default:
            printf("Reserved Frame ID\n");
            break;
    }
    
    return GAS_SENSOR_OK;  /* Return 0 for success */
}

/*
 * Example 1: Simple frame reading with callback
 */
int example_with_callback(const char *serial_port)
{
    printf("\n=== Example 1: Reading frames with callback ===\n");
    
    gas_sensor_handle_t sensor;
    int result;
    
    /* Initialize sensor with callback */
    result = gas_sensor_init(serial_port, sensor_callback, &sensor);
    if (result != GAS_SENSOR_OK) {
        fprintf(stderr, "Failed to initialize sensor: %s\n",
                gas_sensor_strerror(result));
        return 1;
    }
    
    printf("Connected to sensor on %s\n", serial_port);
    printf("Reading frames (press Ctrl+C to stop)...\n");
    
    signal(SIGINT, signal_handler);
    
    /* Read frames until interrupted */
    while (running) {
        result = gas_sensor_read_frame(sensor, NULL, NULL, NULL);
        
        if (result == GAS_SENSOR_OK) {
            /* Frame was successfully read and callback was called */
            printf("Frame received successfully.\n");
        } else if (result != GAS_SENSOR_ERR_SERIAL_READ) {
            /* Log errors but continue */
            fprintf(stderr, "Frame error: %s\n", gas_sensor_strerror(result));
        }
    }
    
    gas_sensor_close(sensor);
    printf("Sensor closed.\n");
    
    return 0;
}

/*
 * Example 2: Reading frames without callback
 */
int example_without_callback(const char *serial_port)
{
    printf("\n=== Example 2: Reading frames without callback ===\n");
    
    gas_sensor_handle_t sensor;
    gas_sensor_slow_data_t slow_data;
    gas_sensor_waveform_t waveform;
    gas_sensor_status_t status;
    int result;
    int frame_count = 0;
    int max_frames = 1000;
    
    /* Initialize sensor without callback */
    result = gas_sensor_init(serial_port, NULL, &sensor);
    if (result != GAS_SENSOR_OK) {
        fprintf(stderr, "Failed to initialize sensor: %s\n",
                gas_sensor_strerror(result));
        return 1;
    }
    
    printf("Connected to sensor on %s\n", serial_port);
    printf("Reading %d frames...\n", max_frames);
    
    /* Read 10 frames */
    while (frame_count < max_frames) {
        result = gas_sensor_read_frame(sensor, &slow_data, &waveform, &status);
        
        if (result == GAS_SENSOR_OK) {
            frame_count++;
            printf("\nFrame %d: FrameID=%d, CO2=%.2f%%, O2=%.2f%%, AA1=%.2f%%\n",
                   frame_count, slow_data.last_frame_id,
                   waveform.co2, waveform.o2, waveform.aa1);
        } else if (result != GAS_SENSOR_ERR_SERIAL_READ) {
            fprintf(stderr, "Frame error: %s\n", gas_sensor_strerror(result));
        }
    }
    
    gas_sensor_close(sensor);
    printf("\nDone.\n");
    
    return 0;
}

/*
 * Example 3: Parsing raw frame data
 */
int example_parse_raw_frame(void)
{
    printf("\n=== Example 3: Parsing raw frame data ===\n");
    
    /* Example frame from log.txt */
    uint8_t frame[] = {
        0xAA, 0x55, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x06, 0x40, 0x00, 0xFF,
        0x04, 0x00, 0x03, 0xF5, 0xBC
    };
    
    gas_sensor_slow_data_t slow_data;
    gas_sensor_waveform_t waveform;
    gas_sensor_status_t status;
    
    gas_sensor_init_slow_data(&slow_data);
    
    int result = gas_sensor_parse_frame(frame, &slow_data, &waveform, &status);
    
    if (result == GAS_SENSOR_OK) {
        printf("Frame parsed successfully!\n");
        printf("FrameID: %d\n", slow_data.last_frame_id);
        printf("Waveform: CO2=%.2f%%, O2=%.2f%%\n", waveform.co2, waveform.o2);
        printf("Checksum: Valid\n");
    } else {
        printf("Parse error: %s\n", gas_sensor_strerror(result));
    }
    
    return 0;
}

/*
 * Main function
 */
int main(int argc, char *argv[])
{
    printf("Anesthetic Gas Sensor API - Examples\n");
    printf("======================================\n");
    
    /* Run example 3 (no hardware required) */
    // example_parse_raw_frame();
    
     // For hardware examples, uncomment one of these:
     // 
     // Linux/macOS:
    //  example_with_callback("/dev/ttyUSB0");
     // 
     // Windows:
     // example_with_callback("COM3");
     // 
     // Or:
     example_without_callback("/dev/ttyUSB0");
    
    return 0;
}
