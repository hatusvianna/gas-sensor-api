/*
 * Anesthetic Gas Sensor API - Implementation
 * 
 * Frame parsing library for Phasein-compatible anesthetic gas sensors.
 * This library is responsible only for parsing raw frame data into structured information.
 * Serial communication (opening ports, reading bytes) is handled by the application layer.
 */

#include "gas_sensor.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * Helper Functions - Frame Parsing Utilities
 * ============================================================================ */

/**
 * Calculate checksum: two's complement of sum of bytes 2-19
 * 
 * Frame structure: [0-1] sync, [2] ID, [3] STS, [4-13] waveform (10 bytes),
 *                  [14-19] slow data (6 bytes), [20] CHK
 * Per protocol: CHK = ~(sum of bytes 2-19) + 1
 */
static uint8_t calculate_checksum(const uint8_t *frame_data)
{
    uint16_t sum = 0;
    
    /* Sum bytes from index 2 to 19 (ID through last slow data byte) */
    for (int i = 2; i < 20; i++) {
        sum += frame_data[i];
    }
    
    /* Two's complement: invert and add 1 */
    return (~sum + 1) & 0xFF;
}

/**
 * Convert raw single-byte concentration value (used for slow data)
 * Single-byte values are encoded as percentage * 10, so 50 = 5.0%
 * 0xFF = invalid marker
 */
static float parse_concentration(uint8_t raw_value)
{
    if (raw_value == GAS_SENSOR_NO_DATA) {
        return GAS_SENSOR_CONC_INVALID;
    }
    /* Convert from percentage*10 to percentage */
    return (float)raw_value / 10.0f;
}

/**
 * Convert raw 2-byte concentration value (used for waveform data)
 * Two-byte values are encoded as percentage * 100, so 500 = 5.0%
 * 0xFFFF = invalid marker
 */
static float parse_concentration_2byte(uint16_t raw_value)
{
    if (raw_value == 0xFFFF) {
        return GAS_SENSOR_CONC_INVALID;
    }
    /* Convert from percentage*100 to percentage */
    return (float)raw_value / 100.0f;
}

/**
 * Parse big-endian 16-bit unsigned integer
 */
static uint16_t parse_uint16_be(const uint8_t *data)
{
    return ((uint16_t)data[0] << 8) | data[1];
}

/* ============================================================================
 * Slow Data Parsing Functions - One per Frame ID
 * ============================================================================ */

/**
 * Parse inspiration values (Frame ID 0x00)
 * Bytes 13-18 of frame: CO2, N2O, AA1, AA2, O2 (5 bytes) + reserved
 */
static void parse_insp_vals(const uint8_t *slow_data_bytes,
                           gas_sensor_insp_vals_t *insp)
{
    insp->co2 = parse_concentration(slow_data_bytes[0]);
    insp->n2o = parse_concentration(slow_data_bytes[1]);
    insp->aa1 = parse_concentration(slow_data_bytes[2]);
    insp->aa2 = parse_concentration(slow_data_bytes[3]);
    insp->o2 = parse_concentration(slow_data_bytes[4]);
}

/**
 * Parse expiration values (Frame ID 0x01)
 * Bytes 13-18 of frame: CO2, N2O, AA1, AA2, O2 (5 bytes) + reserved
 */
static void parse_exp_vals(const uint8_t *slow_data_bytes,
                          gas_sensor_exp_vals_t *exp)
{
    exp->co2 = parse_concentration(slow_data_bytes[0]);
    exp->n2o = parse_concentration(slow_data_bytes[1]);
    exp->aa1 = parse_concentration(slow_data_bytes[2]);
    exp->aa2 = parse_concentration(slow_data_bytes[3]);
    exp->o2 = parse_concentration(slow_data_bytes[4]);
}

/**
 * Parse momentary values (Frame ID 0x02)
 * Bytes 13-18 of frame: CO2, N2O, AA1, AA2, O2 (5 bytes) + reserved
 */
static void parse_mom_vals(const uint8_t *slow_data_bytes,
                          gas_sensor_mom_vals_t *mom)
{
    mom->co2 = parse_concentration(slow_data_bytes[0]);
    mom->n2o = parse_concentration(slow_data_bytes[1]);
    mom->aa1 = parse_concentration(slow_data_bytes[2]);
    mom->aa2 = parse_concentration(slow_data_bytes[3]);
    mom->o2 = parse_concentration(slow_data_bytes[4]);
}

/**
 * Parse general values (Frame ID 0x03)
 * Byte 13: Respiratory rate (0xFF = invalid)
 * Byte 14: Time since breath (0xFF = invalid)
 * Byte 15: Agent identification
 * Byte 16: Secondary agent identification
 * Bytes 17-18: Atmospheric pressure (big-endian, 0xFFFF = invalid)
 */
static void parse_gen_vals(const uint8_t *slow_data_bytes,
                          gas_sensor_gen_vals_t *gen)
{
    gen->resp_rate = slow_data_bytes[0];
    gen->time_since_breath = slow_data_bytes[1];
    gen->primary_agent = (gas_agent_id_t)slow_data_bytes[2];
    gen->secondary_agent = (gas_agent_id_t)slow_data_bytes[3];
    
    uint16_t pressure_raw = parse_uint16_be(&slow_data_bytes[4]);
    if (pressure_raw == 0xFFFF) {
        gen->atm_pressure = GAS_SENSOR_CONC_INVALID;
    } else {
        gen->atm_pressure = (float)pressure_raw / 100.0f;  /* In kPa */
    }
}

/**
 * Parse sensor registers (Frame ID 0x04)
 * Byte 13: Mode
 * Byte 14: Error register
 * Byte 15: Adapter status register
 * Byte 16: Data valid register
 * Byte 17: Reserved
 * Byte 18: Reserved
 */
static void parse_sensor_regs(const uint8_t *slow_data_bytes,
                             gas_sensor_sensor_regs_t *regs)
{
    /* Mode */
    regs->mode = (gas_sensor_mode_t)slow_data_bytes[0];
    
    /* Error register (byte 14) */
    uint8_t error_byte = slow_data_bytes[1];
    regs->error.sw_error = (error_byte & 0x01) != 0;
    regs->error.hw_error = (error_byte & 0x02) != 0;
    regs->error.motor_fail = (error_byte & 0x04) != 0;
    regs->error.uncalibrated = (error_byte & 0x08) != 0;
    
    /* Adapter status register (byte 15) */
    uint8_t adapter_byte = slow_data_bytes[2];
    regs->adapter.replace_adapter = (adapter_byte & 0x01) != 0;
    regs->adapter.no_adapter = (adapter_byte & 0x02) != 0;
    regs->adapter.o2_clogged = (adapter_byte & 0x04) != 0;
    
    /* Data valid register (byte 16) */
    uint8_t valid_byte = slow_data_bytes[3];
    regs->data_valid.co2_out_of_range = (valid_byte & 0x01) != 0;
    regs->data_valid.n2o_out_of_range = (valid_byte & 0x02) != 0;
    regs->data_valid.agent_out_of_range = (valid_byte & 0x04) != 0;
    regs->data_valid.o2_out_of_range = (valid_byte & 0x08) != 0;
    regs->data_valid.temp_out_of_range = (valid_byte & 0x10) != 0;
    regs->data_valid.pressure_out_of_range = (valid_byte & 0x20) != 0;
    regs->data_valid.zero_calibration_required = (valid_byte & 0x40) != 0;
}

/**
 * Parse configuration data (Frame ID 0x05)
 * Byte 13: Fitted options (bit flags)
 * Byte 14: Reserved
 * Bytes 15-16: Hardware revision (big-endian)
 * Bytes 17-18: Software revision (big-endian)
 */
static void parse_config_data(const uint8_t *slow_data_bytes,
                             gas_sensor_config_data_t *config)
{
    /* Fitted options (byte 13) */
    uint8_t fitted_byte = slow_data_bytes[0];
    config->o2_fitted = (fitted_byte & 0x01) != 0;
    config->co2_fitted = (fitted_byte & 0x02) != 0;
    config->n2o_fitted = (fitted_byte & 0x04) != 0;
    config->halothane_fitted = (fitted_byte & 0x08) != 0;
    config->enflurane_fitted = (fitted_byte & 0x10) != 0;
    config->isoflurane_fitted = (fitted_byte & 0x20) != 0;
    config->sevoflurane_fitted = (fitted_byte & 0x40) != 0;
    config->desflurane_fitted = (fitted_byte & 0x80) != 0;
    
    /* Hardware revision (bytes 15-16) */
    config->hw_revision = parse_uint16_be(&slow_data_bytes[2]);
    
    /* Software revision (bytes 17-18) */
    config->sw_revision = parse_uint16_be(&slow_data_bytes[4]);
    
    /* Configuration flags */
    config->id_config = false;  /* Would need additional info to determine */
    config->comm_protocol_rev = 0;  /* Would need additional info */
}

/**
 * Parse service data (Frame ID 0x06)
 * Bytes 13-14: Serial number (big-endian)
 * Byte 15: Service status register
 * Bytes 16-18: Reserved
 */
static void parse_service_data(const uint8_t *slow_data_bytes,
                              gas_sensor_service_data_t *service)
{
    /* Serial number (bytes 13-14) */
    service->serial_number = parse_uint16_be(&slow_data_bytes[0]);
    
    /* Service status register (byte 15) */
    uint8_t status_byte = slow_data_bytes[2];
    service->status.zero_disabled = (status_byte & 0x01) != 0;
    service->status.zero_in_progress = (status_byte & 0x02) != 0;
    service->status.span_calibration_error = (status_byte & 0x04) != 0;
    service->status.span_calibration_in_progress = (status_byte & 0x08) != 0;
}

/* ============================================================================
 * Public API Functions
 * ============================================================================ */

int gas_sensor_parse_frame(const uint8_t *frame_data,
                          gas_sensor_slow_data_t *slow_data,
                          gas_sensor_waveform_t *waveform,
                          gas_sensor_status_t *status)
{
    /* Parameter validation */
    if (frame_data == NULL) {
        return GAS_SENSOR_ERR_NULL_PARAM;
    }
    
    /* Validate frame synchronization bytes */
    if (frame_data[0] != GAS_SENSOR_FLAG1 || frame_data[1] != GAS_SENSOR_FLAG2) {
        return GAS_SENSOR_ERR_INVALID_FRAME;
    }
    
    /* Validate checksum */
    if (!gas_sensor_verify_checksum(frame_data)) {
        return GAS_SENSOR_ERR_CHECKSUM;
    }
    
    /* Parse waveform data (bytes 4-13: 10 bytes for 5 concentrations × 2 bytes each, big-endian) */
    if (waveform != NULL) {
        waveform->co2 = parse_concentration_2byte(parse_uint16_be(&frame_data[4]));
        waveform->n2o = parse_concentration_2byte(parse_uint16_be(&frame_data[6]));
        waveform->aa1 = parse_concentration_2byte(parse_uint16_be(&frame_data[8]));
        waveform->aa2 = parse_concentration_2byte(parse_uint16_be(&frame_data[10]));
        waveform->o2 = parse_concentration_2byte(parse_uint16_be(&frame_data[12]));
    }
    
    /* Parse status byte (byte 3) */
    if (status != NULL) {
        uint8_t status_byte = frame_data[3];
        status->breath_detected = (status_byte & 0x01) != 0;
        status->apnea = (status_byte & 0x02) != 0;
        status->o2_low = (status_byte & 0x04) != 0;
        status->o2_replace = (status_byte & 0x08) != 0;
        status->check_adapter = (status_byte & 0x10) != 0;
        status->accuracy_out_of_range = (status_byte & 0x20) != 0;
        status->sensor_error = (status_byte & 0x40) != 0;
        status->o2_calibration_required = (status_byte & 0x80) != 0;
    }
    
    /* Parse slow data based on frame ID (byte 2) */
    if (slow_data != NULL) {
        uint8_t frame_id = frame_data[2];
        
        /* Validate frame ID */
        if (frame_id >= GAS_SENSOR_FRAME_ID_MAX) {
            return GAS_SENSOR_ERR_INVALID_FRAME;
        }
        
        slow_data->last_frame_id = frame_id;
        
        /* Bytes 13-18 contain slow data */
        const uint8_t *slow_data_bytes = &frame_data[13];
        
        switch (frame_id) {
            case 0x00:
                parse_insp_vals(slow_data_bytes, &slow_data->insp_vals);
                break;
            case 0x01:
                parse_exp_vals(slow_data_bytes, &slow_data->exp_vals);
                break;
            case 0x02:
                parse_mom_vals(slow_data_bytes, &slow_data->mom_vals);
                break;
            case 0x03:
                parse_gen_vals(slow_data_bytes, &slow_data->gen_vals);
                break;
            case 0x04:
                parse_sensor_regs(slow_data_bytes, &slow_data->sensor_regs);
                break;
            case 0x05:
                parse_config_data(slow_data_bytes, &slow_data->config_data);
                break;
            case 0x06:
                parse_service_data(slow_data_bytes, &slow_data->service_data);
                break;
            case 0x07:
            case 0x08:
            case 0x09:
                /* Reserved frame IDs - no action */
                break;
        }
    }
    
    return GAS_SENSOR_OK;
}

bool gas_sensor_verify_checksum(const uint8_t *frame_data)
{
    if (frame_data == NULL) {
        return false;
    }
    
    /* Frame must have sync bytes */
    if (frame_data[0] != GAS_SENSOR_FLAG1 || frame_data[1] != GAS_SENSOR_FLAG2) {
        return false;
    }
    
    /* Calculate expected checksum */
    uint8_t calculated = calculate_checksum(frame_data);
    
    /* Get checksum from frame (byte 20, the last byte) */
    uint8_t frame_checksum = frame_data[20];
    
    return calculated == frame_checksum;
}

void gas_sensor_init_slow_data(gas_sensor_slow_data_t *slow_data)
{
    if (slow_data == NULL) {
        return;
    }
    
    memset(slow_data, 0, sizeof(gas_sensor_slow_data_t));
    
    /* Initialize all float fields to invalid */
    slow_data->insp_vals.co2 = GAS_SENSOR_CONC_INVALID;
    slow_data->insp_vals.n2o = GAS_SENSOR_CONC_INVALID;
    slow_data->insp_vals.aa1 = GAS_SENSOR_CONC_INVALID;
    slow_data->insp_vals.aa2 = GAS_SENSOR_CONC_INVALID;
    slow_data->insp_vals.o2 = GAS_SENSOR_CONC_INVALID;
    
    slow_data->exp_vals.co2 = GAS_SENSOR_CONC_INVALID;
    slow_data->exp_vals.n2o = GAS_SENSOR_CONC_INVALID;
    slow_data->exp_vals.aa1 = GAS_SENSOR_CONC_INVALID;
    slow_data->exp_vals.aa2 = GAS_SENSOR_CONC_INVALID;
    slow_data->exp_vals.o2 = GAS_SENSOR_CONC_INVALID;
    
    slow_data->mom_vals.co2 = GAS_SENSOR_CONC_INVALID;
    slow_data->mom_vals.n2o = GAS_SENSOR_CONC_INVALID;
    slow_data->mom_vals.aa1 = GAS_SENSOR_CONC_INVALID;
    slow_data->mom_vals.aa2 = GAS_SENSOR_CONC_INVALID;
    slow_data->mom_vals.o2 = GAS_SENSOR_CONC_INVALID;
    
    slow_data->gen_vals.atm_pressure = GAS_SENSOR_CONC_INVALID;
}

float gas_sensor_parse_concentration(uint8_t raw_value)
{
    return parse_concentration(raw_value);
}

const char *gas_sensor_strerror(int error_code)
{
    switch (error_code) {
        case GAS_SENSOR_OK:
            return "No error";
        case GAS_SENSOR_ERR_INVALID_FRAME:
            return "Invalid frame (bad sync bytes or frame ID)";
        case GAS_SENSOR_ERR_CHECKSUM:
            return "Checksum verification failed";
        case GAS_SENSOR_ERR_NULL_PARAM:
            return "NULL parameter provided";
        default:
            return "Unknown error";
    }
}
