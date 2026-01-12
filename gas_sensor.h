/*
 * Anesthetic Gas Sensor API
 * 
 * Serial communication with Phasein (Masimo) compatible gas analyzer
 * Protocol: 9600 baud, 8 data bits, no parity, 1 stop bit
 * Frame: 21 bytes, transmitted every 50ms
 */

#ifndef GAS_SENSOR_H
#define GAS_SENSOR_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Error Codes
 * ============================================================================ */

#define GAS_SENSOR_OK                    0
#define GAS_SENSOR_ERR_INVALID_FRAME    -1
#define GAS_SENSOR_ERR_CHECKSUM         -2
#define GAS_SENSOR_ERR_SERIAL_OPEN      -3
#define GAS_SENSOR_ERR_SERIAL_READ      -4
#define GAS_SENSOR_ERR_SERIAL_WRITE     -5
#define GAS_SENSOR_ERR_CALLBACK         -6
#define GAS_SENSOR_ERR_NULL_PARAM       -7
#define GAS_SENSOR_ERR_MEMORY           -8

/* ============================================================================
 * Constants
 * ============================================================================ */

#define GAS_SENSOR_FRAME_SIZE           21
#define GAS_SENSOR_FLAG1                0xAA
#define GAS_SENSOR_FLAG2                0x55
#define GAS_SENSOR_FRAME_ID_MAX         10

#define GAS_SENSOR_NO_DATA              0xFF

/* Special value for "no data" - represents missing measurement */
#define GAS_SENSOR_CONC_INVALID         -1.0f

/* ============================================================================
 * Enumerations
 * ============================================================================ */

typedef enum {
    GAS_SENSOR_MODE_SELF_TEST = 0,
    GAS_SENSOR_MODE_SLEEP = 1,
    GAS_SENSOR_MODE_MEASUREMENT = 2,
    GAS_SENSOR_MODE_DEMO = 3
} gas_sensor_mode_t;

typedef enum {
    GAS_AGENT_NONE = 0,
    GAS_AGENT_HALOTHANE = 1,
    GAS_AGENT_ENFLURANE = 2,
    GAS_AGENT_ISOFLURANE = 3,
    GAS_AGENT_SEVOFLURANE = 4,
    GAS_AGENT_DESFLURANE = 5
} gas_agent_id_t;

/* ============================================================================
 * Fast Data Structure (Waveform Data)
 * 
 * Updated every 50ms with current gas concentrations
 * ============================================================================ */

typedef struct {
    float co2;          /* CO2 concentration (%) */
    float n2o;          /* N2O concentration (%) */
    float aa1;          /* Anesthetic Agent 1 concentration (%) */
    float aa2;          /* Anesthetic Agent 2 concentration (%) */
    float o2;           /* O2 concentration (%) */
} gas_sensor_waveform_t;

/* ============================================================================
 * Status Summary Structure
 * 
 * Status byte interpretation (updated every 50ms)
 * ============================================================================ */

typedef struct {
    bool breath_detected;           /* Bit 0: Breath detected */
    bool apnea;                     /* Bit 1: Apnea detected */
    bool o2_low;                    /* Bit 2: O2 sensor low sensitivity */
    bool o2_replace;                /* Bit 3: Replace O2 sensor */
    bool check_adapter;             /* Bit 4: Check adapter */
    bool accuracy_out_of_range;     /* Bit 5: Accuracy out of range */
    bool sensor_error;              /* Bit 6: Sensor error */
    bool o2_calibration_required;   /* Bit 7: O2 calibration required */
} gas_sensor_status_t;

/* ============================================================================
 * Slow Data Structures
 * 
 * Updated every 500ms (one field per 50ms frame)
 * ============================================================================ */

/* ID 0x00: Inspiration values */
typedef struct {
    float co2;          /* CO2 inspiration concentration (%) */
    float n2o;          /* N2O inspiration concentration (%) */
    float aa1;          /* AA1 inspiration concentration (%) */
    float aa2;          /* AA2 inspiration concentration (%) */
    float o2;           /* O2 inspiration concentration (%) */
} gas_sensor_insp_vals_t;

/* ID 0x01: Expiration values */
typedef struct {
    float co2;          /* CO2 expiration concentration (%) */
    float n2o;          /* N2O expiration concentration (%) */
    float aa1;          /* AA1 expiration concentration (%) */
    float aa2;          /* AA2 expiration concentration (%) */
    float o2;           /* O2 expiration concentration (%) */
} gas_sensor_exp_vals_t;

/* ID 0x02: Momentary values */
typedef struct {
    float co2;          /* CO2 momentary concentration (%) */
    float n2o;          /* N2O momentary concentration (%) */
    float aa1;          /* AA1 momentary concentration (%) */
    float aa2;          /* AA2 momentary concentration (%) */
    float o2;           /* O2 momentary concentration (%) */
} gas_sensor_mom_vals_t;

/* ID 0x03: General values */
typedef struct {
    uint8_t resp_rate;              /* Respiratory rate (bpm), 0xFF = invalid */
    uint8_t time_since_breath;      /* Time since last breath (s), 0xFF = invalid */
    gas_agent_id_t primary_agent;   /* Primary anesthetic agent ID */
    gas_agent_id_t secondary_agent; /* Secondary anesthetic agent ID */
    float atm_pressure;             /* Atmospheric pressure (kPa), -1.0 = invalid */
} gas_sensor_gen_vals_t;

/* Sensor error register (ID 0x04, byte 2) */
typedef struct {
    bool sw_error;                  /* Software error */
    bool hw_error;                  /* Hardware error */
    bool motor_fail;                /* Motor speed out of bounds */
    bool uncalibrated;              /* Factory calibration lost */
} gas_sensor_error_reg_t;

/* Adapter status register (ID 0x04, byte 3) */
typedef struct {
    bool replace_adapter;           /* Replace adapter (IR signal low) */
    bool no_adapter;                /* No adapter connected (IR signal high) */
    bool o2_clogged;                /* O2 port clogged/plugged */
} gas_sensor_adapter_reg_t;

/* Data valid register (ID 0x04, byte 4) */
typedef struct {
    bool co2_out_of_range;          /* CO2 outside [0-10]% range */
    bool n2o_out_of_range;          /* N2O outside [0-100]% range */
    bool agent_out_of_range;        /* At least one agent out of range */
    bool o2_out_of_range;           /* O2 outside [10-100]% range */
    bool temp_out_of_range;         /* Internal temp outside [10-50]°C */
    bool pressure_out_of_range;     /* Ambient pressure outside [70-120]kPa */
    bool zero_calibration_required; /* Negative concentrations detected */
} gas_sensor_data_valid_reg_t;

/* ID 0x04: Sensor registers */
typedef struct {
    gas_sensor_mode_t mode;
    gas_sensor_error_reg_t error;
    gas_sensor_adapter_reg_t adapter;
    gas_sensor_data_valid_reg_t data_valid;
} gas_sensor_sensor_regs_t;

/* ID 0x05: Configuration data */
typedef struct {
    bool o2_fitted;                 /* O2 option fitted */
    bool co2_fitted;                /* CO2 option fitted */
    bool n2o_fitted;                /* N2O option fitted */
    bool halothane_fitted;          /* Halothane option fitted */
    bool enflurane_fitted;          /* Enflurane option fitted */
    bool isoflurane_fitted;         /* Isoflurane option fitted */
    bool sevoflurane_fitted;        /* Sevoflurane option fitted */
    bool desflurane_fitted;         /* Desflurane option fitted */
    uint16_t hw_revision;           /* Hardware revision */
    uint16_t sw_revision;           /* Software revision */
    bool id_config;                 /* Agent ID auto-identification enabled */
    uint8_t comm_protocol_rev;      /* Communication protocol revision */
} gas_sensor_config_data_t;

/* Service status register (ID 0x06, byte 2) */
typedef struct {
    bool zero_disabled;                     /* Zero calibration disabled */
    bool zero_in_progress;                  /* Zero calibration in progress */
    bool span_calibration_error;            /* O2 span calibration failure */
    bool span_calibration_in_progress;      /* O2 span calibration in progress */
} gas_sensor_service_status_t;

/* ID 0x06: Service data */
typedef struct {
    uint16_t serial_number;
    gas_sensor_service_status_t status;
} gas_sensor_service_data_t;

/* ============================================================================
 * Complete Slow Data Structure
 * 
 * Aggregates all slow data fields. Only relevant fields are updated
 * based on the frame ID (0-9). Other fields retain previous values.
 * ============================================================================ */

typedef struct {
    uint8_t last_frame_id;                  /* Last frame ID (0-9) */
    
    /* ID 0x00 */
    gas_sensor_insp_vals_t insp_vals;
    
    /* ID 0x01 */
    gas_sensor_exp_vals_t exp_vals;
    
    /* ID 0x02 */
    gas_sensor_mom_vals_t mom_vals;
    
    /* ID 0x03 */
    gas_sensor_gen_vals_t gen_vals;
    
    /* ID 0x04 */
    gas_sensor_sensor_regs_t sensor_regs;
    
    /* ID 0x05 */
    gas_sensor_config_data_t config_data;
    
    /* ID 0x06 */
    gas_sensor_service_data_t service_data;
    
    /* Note: IDs 0x07-0x09 are reserved (no data) */
} gas_sensor_slow_data_t;

/* ============================================================================
 * Callback Function Prototype
 * 
 * Called when a complete frame is received and parsed successfully.
 * Parameters:
 *   - slow_data:   Pointer to slow data structure (updated with new fields)
 *   - waveform:    Pointer to waveform (fast) data structure
 *   - status:      Pointer to status summary structure
 * 
 * Returns:
 *   - 0:  Success
 *   - <0: Error code (processing should continue but error is logged)
 * ============================================================================ */

typedef int (*gas_sensor_callback_t)(
    gas_sensor_slow_data_t *slow_data,
    gas_sensor_waveform_t *waveform,
    gas_sensor_status_t *status
);

/* ============================================================================
 * Main Sensor Handle
 * 
 * Opaque handle for sensor communication state
 * ============================================================================ */

typedef struct gas_sensor_handle *gas_sensor_handle_t;

/* ============================================================================
 * Public API Functions
 * ============================================================================ */

/*
 * Initialize the gas sensor communication
 * 
 * Parameters:
 *   - port: Serial port name (e.g., "/dev/ttyUSB0" on Linux, "COM3" on Windows)
 *   - callback: Function to call when a frame is received, or NULL
 *   - handle: Pointer to store the sensor handle
 * 
 * Returns:
 *   - GAS_SENSOR_OK: Success
 *   - GAS_SENSOR_ERR_SERIAL_OPEN: Failed to open serial port
 *   - GAS_SENSOR_ERR_MEMORY: Memory allocation failed
 */
int gas_sensor_init(const char *port, gas_sensor_callback_t callback,
                    gas_sensor_handle_t *handle);

/*
 * Close the sensor and clean up resources
 * 
 * Parameters:
 *   - handle: Sensor handle to close
 * 
 * Returns:
 *   - GAS_SENSOR_OK: Success
 */
int gas_sensor_close(gas_sensor_handle_t handle);

/*
 * Read and process one frame from the sensor
 * 
 * Blocks until a complete frame is received. If a callback is registered,
 * it will be called when a valid frame is parsed.
 * 
 * Parameters:
 *   - handle: Sensor handle
 *   - slow_data: Pointer to slow data structure (optional, can be NULL)
 *   - waveform: Pointer to waveform data structure (optional, can be NULL)
 *   - status: Pointer to status structure (optional, can be NULL)
 * 
 * Returns:
 *   - GAS_SENSOR_OK: Frame received and parsed successfully
 *   - GAS_SENSOR_ERR_SERIAL_READ: Serial read error
 *   - GAS_SENSOR_ERR_INVALID_FRAME: Invalid frame detected
 *   - GAS_SENSOR_ERR_CHECKSUM: Checksum verification failed
 */
int gas_sensor_read_frame(gas_sensor_handle_t handle,
                         gas_sensor_slow_data_t *slow_data,
                         gas_sensor_waveform_t *waveform,
                         gas_sensor_status_t *status);

/*
 * Parse a raw frame buffer
 * 
 * Parameters:
 *   - frame_data: Pointer to 21-byte frame buffer
 *   - slow_data: Pointer to slow data structure
 *   - waveform: Pointer to waveform data structure
 *   - status: Pointer to status structure
 * 
 * Returns:
 *   - GAS_SENSOR_OK: Frame parsed successfully
 *   - GAS_SENSOR_ERR_INVALID_FRAME: Invalid frame
 *   - GAS_SENSOR_ERR_CHECKSUM: Checksum failed
 */
int gas_sensor_parse_frame(const uint8_t *frame_data,
                          gas_sensor_slow_data_t *slow_data,
                          gas_sensor_waveform_t *waveform,
                          gas_sensor_status_t *status);

/*
 * Verify frame checksum
 * 
 * Parameters:
 *   - frame_data: Pointer to 21-byte frame buffer
 * 
 * Returns:
 *   - true: Checksum is valid
 *   - false: Checksum failed
 */
bool gas_sensor_verify_checksum(const uint8_t *frame_data);

/*
 * Initialize slow data structure with default values
 * 
 * Parameters:
 *   - slow_data: Pointer to structure to initialize
 */
void gas_sensor_init_slow_data(gas_sensor_slow_data_t *slow_data);

/*
 * Convert concentration value (0xFF = invalid)
 * 
 * Parameters:
 *   - raw_value: Raw value from sensor (0-255)
 * 
 * Returns:
 *   - Concentration in percent, or GAS_SENSOR_CONC_INVALID (-1.0f)
 */
float gas_sensor_parse_concentration(uint8_t raw_value);

/*
 * Get human-readable error message
 * 
 * Parameters:
 *   - error_code: Error code from API function
 * 
 * Returns:
 *   - Pointer to static error message string
 */
const char *gas_sensor_strerror(int error_code);

#ifdef __cplusplus
}
#endif

#endif /* GAS_SENSOR_H */
