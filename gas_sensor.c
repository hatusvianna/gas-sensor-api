/*
 * Anesthetic Gas Sensor API - Implementation
 */

#include "gas_sensor.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <termios.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <sys/select.h>
#endif

/* ============================================================================
 * Internal Sensor Handle Structure
 * ============================================================================ */

struct gas_sensor_handle {
#ifdef _WIN32
    HANDLE serial_port;
#else
    int serial_fd;
#endif
    gas_sensor_callback_t callback;
    gas_sensor_slow_data_t slow_data;
    uint8_t rx_buffer[256];         /* Receive buffer for frame synchronization */
    int rx_buffer_len;              /* Current number of bytes in rx_buffer */
};

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/* Calculate checksum: two's complement of sum of bytes 2-19 */
static uint8_t calculate_checksum(const uint8_t *frame_data)
{
    uint16_t sum = 0;
    
    /* Sum bytes from index 2 to 19 (exclude FLAG1, FLAG2, and CHK) */
    for (int i = 2; i < 20; i++) {
        sum += frame_data[i];
    }
    
    /* Two's complement */
    return (~sum + 1) & 0xFF;
}

/* Parse a single concentration value */
static float parse_concentration(uint8_t raw_value)
{
    if (raw_value == GAS_SENSOR_NO_DATA) {
        return GAS_SENSOR_CONC_INVALID;
    }
    return (float)raw_value;
}

/* Parse a 16-bit big-endian value */
static uint16_t parse_uint16_be(const uint8_t *data)
{
    return ((uint16_t)data[0] << 8) | data[1];
}

/* Parse inspiration values (ID 0x00) */
static void parse_insp_vals(const uint8_t *slow_data,
                           gas_sensor_insp_vals_t *insp)
{
    insp->co2 = parse_concentration(slow_data[0]);
    insp->n2o = parse_concentration(slow_data[1]);
    insp->aa1 = parse_concentration(slow_data[2]);
    insp->aa2 = parse_concentration(slow_data[3]);
    insp->o2 = parse_concentration(slow_data[4]);
}

/* Parse expiration values (ID 0x01) */
static void parse_exp_vals(const uint8_t *slow_data,
                          gas_sensor_exp_vals_t *exp)
{
    exp->co2 = parse_concentration(slow_data[0]);
    exp->n2o = parse_concentration(slow_data[1]);
    exp->aa1 = parse_concentration(slow_data[2]);
    exp->aa2 = parse_concentration(slow_data[3]);
    exp->o2 = parse_concentration(slow_data[4]);
}

/* Parse momentary values (ID 0x02) */
static void parse_mom_vals(const uint8_t *slow_data,
                          gas_sensor_mom_vals_t *mom)
{
    mom->co2 = parse_concentration(slow_data[0]);
    mom->n2o = parse_concentration(slow_data[1]);
    mom->aa1 = parse_concentration(slow_data[2]);
    mom->aa2 = parse_concentration(slow_data[3]);
    mom->o2 = parse_concentration(slow_data[4]);
}

/* Parse general values (ID 0x03) */
static void parse_gen_vals(const uint8_t *slow_data,
                          gas_sensor_gen_vals_t *gen)
{
    gen->resp_rate = (slow_data[0] == GAS_SENSOR_NO_DATA) ? GAS_SENSOR_NO_DATA : slow_data[0];
    gen->time_since_breath = (slow_data[1] == GAS_SENSOR_NO_DATA) ? GAS_SENSOR_NO_DATA : slow_data[1];
    gen->primary_agent = (gas_agent_id_t)((slow_data[2] == GAS_SENSOR_NO_DATA) ? 0 : slow_data[2]);
    gen->secondary_agent = (gas_agent_id_t)((slow_data[3] == GAS_SENSOR_NO_DATA) ? 0 : slow_data[3]);
    
    if (slow_data[4] == GAS_SENSOR_NO_DATA || slow_data[5] == GAS_SENSOR_NO_DATA) {
        gen->atm_pressure = GAS_SENSOR_CONC_INVALID;
    } else {
        uint16_t raw_pressure = parse_uint16_be(&slow_data[4]);
        gen->atm_pressure = raw_pressure / 10.0f;  /* Convert to kPa */
    }
}

/* Parse sensor registers (ID 0x04) */
static void parse_sensor_regs(const uint8_t *slow_data,
                             gas_sensor_sensor_regs_t *regs)
{
    /* Byte 0: Mode register */
    regs->mode = (gas_sensor_mode_t)(slow_data[0] & 0x07);
    
    /* Byte 2: Error register */
    regs->error.sw_error = (slow_data[2] & 0x01) != 0;
    regs->error.hw_error = (slow_data[2] & 0x02) != 0;
    regs->error.motor_fail = (slow_data[2] & 0x04) != 0;
    regs->error.uncalibrated = (slow_data[2] & 0x08) != 0;
    
    /* Byte 3: Adapter status register */
    regs->adapter.replace_adapter = (slow_data[3] & 0x01) != 0;
    regs->adapter.no_adapter = (slow_data[3] & 0x02) != 0;
    regs->adapter.o2_clogged = (slow_data[3] & 0x04) != 0;
    
    /* Byte 4: Data valid register */
    regs->data_valid.co2_out_of_range = (slow_data[4] & 0x01) != 0;
    regs->data_valid.n2o_out_of_range = (slow_data[4] & 0x02) != 0;
    regs->data_valid.agent_out_of_range = (slow_data[4] & 0x04) != 0;
    regs->data_valid.o2_out_of_range = (slow_data[4] & 0x08) != 0;
    regs->data_valid.temp_out_of_range = (slow_data[4] & 0x10) != 0;
    regs->data_valid.pressure_out_of_range = (slow_data[4] & 0x20) != 0;
    regs->data_valid.zero_calibration_required = (slow_data[4] & 0x40) != 0;
}

/* Parse configuration data (ID 0x05) */
static void parse_config_data(const uint8_t *slow_data,
                             gas_sensor_config_data_t *config)
{
    /* Byte 0: Configuration register 0 */
    config->o2_fitted = (slow_data[0] & 0x01) != 0;
    config->co2_fitted = (slow_data[0] & 0x02) != 0;
    config->n2o_fitted = (slow_data[0] & 0x04) != 0;
    config->halothane_fitted = (slow_data[0] & 0x08) != 0;
    config->enflurane_fitted = (slow_data[0] & 0x10) != 0;
    config->isoflurane_fitted = (slow_data[0] & 0x20) != 0;
    config->sevoflurane_fitted = (slow_data[0] & 0x40) != 0;
    config->desflurane_fitted = (slow_data[0] & 0x80) != 0;
    
    /* Byte 1-2: Hardware and software revision */
    config->hw_revision = slow_data[1];
    config->sw_revision = parse_uint16_be(&slow_data[2]);
    
    /* Byte 5: Configuration register 1 */
    config->id_config = (slow_data[5] & 0x01) != 0;
    config->comm_protocol_rev = (slow_data[5] >> 1) & 0x7F;
}

/* Parse service data (ID 0x06) */
static void parse_service_data(const uint8_t *slow_data,
                              gas_sensor_service_data_t *service)
{
    /* Bytes 0-1: Serial number */
    service->serial_number = parse_uint16_be(slow_data);
    
    /* Byte 2: Service status register */
    service->status.zero_disabled = (slow_data[2] & 0x01) != 0;
    service->status.zero_in_progress = (slow_data[2] & 0x02) != 0;
    service->status.span_calibration_error = (slow_data[2] & 0x04) != 0;
    service->status.span_calibration_in_progress = (slow_data[2] & 0x08) != 0;
}

/* ============================================================================
 * Public API Implementation
 * ============================================================================ */

int gas_sensor_init(const char *port, gas_sensor_callback_t callback,
                   gas_sensor_handle_t *handle)
{
    if (!port || !handle) {
        return GAS_SENSOR_ERR_NULL_PARAM;
    }
    
    /* Allocate sensor handle */
    struct gas_sensor_handle *h = malloc(sizeof(struct gas_sensor_handle));
    if (!h) {
        return GAS_SENSOR_ERR_MEMORY;
    }
    
    h->callback = callback;
    gas_sensor_init_slow_data(&h->slow_data);
    h->rx_buffer_len = 0;  /* Initialize receive buffer */
    
#ifdef _WIN32
    /* Windows serial port initialization */
    h->serial_port = CreateFileA(
        port,
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        0,
        NULL
    );
    
    if (h->serial_port == INVALID_HANDLE_VALUE) {
        free(h);
        return GAS_SENSOR_ERR_SERIAL_OPEN;
    }
    
    DCB dcb = {0};
    dcb.DCBlength = sizeof(dcb);
    GetCommState(h->serial_port, &dcb);
    dcb.BaudRate = CBR_9600;
    dcb.ByteSize = 8;
    dcb.StopBits = ONESTOPBIT;
    dcb.Parity = NOPARITY;
    SetCommState(h->serial_port, &dcb);
    
#else
    /* POSIX serial port initialization */
    h->serial_fd = open(port, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (h->serial_fd < 0) {
        free(h);
        return GAS_SENSOR_ERR_SERIAL_OPEN;
    }
    
    struct termios tty;
    if (tcgetattr(h->serial_fd, &tty) != 0) {
        close(h->serial_fd);
        free(h);
        return GAS_SENSOR_ERR_SERIAL_OPEN;
    }
    
    /* Configure serial port: 9600 baud, 8 data bits, no parity, 1 stop bit */
    tty.c_cflag = CS8 | CREAD | CLOCAL;
    tty.c_iflag = 0;
    tty.c_oflag = 0;
    tty.c_lflag = 0;
    tty.c_cc[VTIME] = 10;  /* 1 second timeout */
    tty.c_cc[VMIN] = 0;
    
    cfsetispeed(&tty, B9600);
    cfsetospeed(&tty, B9600);
    
    if (tcsetattr(h->serial_fd, TCSANOW, &tty) != 0) {
        close(h->serial_fd);
        free(h);
        return GAS_SENSOR_ERR_SERIAL_OPEN;
    }
    
    tcflush(h->serial_fd, TCIOFLUSH);
#endif
    
    *handle = h;
    return GAS_SENSOR_OK;
}

int gas_sensor_close(gas_sensor_handle_t handle)
{
    if (!handle) {
        return GAS_SENSOR_ERR_NULL_PARAM;
    }
    
    struct gas_sensor_handle *h = (struct gas_sensor_handle *)handle;
    
#ifdef _WIN32
    CloseHandle(h->serial_port);
#else
    if (h->serial_fd >= 0) {
        close(h->serial_fd);
    }
#endif
    
    free(h);
    return GAS_SENSOR_OK;
}

bool gas_sensor_verify_checksum(const uint8_t *frame_data)
{
    if (!frame_data) {
        return false;
    }
    
    uint8_t expected = frame_data[20];
    uint8_t calculated = calculate_checksum(frame_data);
    
    return expected == calculated;
}

int gas_sensor_parse_frame(const uint8_t *frame_data,
                          gas_sensor_slow_data_t *slow_data,
                          gas_sensor_waveform_t *waveform,
                          gas_sensor_status_t *status)
{
    if (!frame_data) {
        return GAS_SENSOR_ERR_NULL_PARAM;
    }
    
    /* Verify frame flags */
    if (frame_data[0] != GAS_SENSOR_FLAG1 || frame_data[1] != GAS_SENSOR_FLAG2) {
        return GAS_SENSOR_ERR_INVALID_FRAME;
    }
    
    /* Verify checksum */
    if (!gas_sensor_verify_checksum(frame_data)) {
        return GAS_SENSOR_ERR_CHECKSUM;
    }
    
    uint8_t frame_id = frame_data[2];
    uint8_t status_byte = frame_data[3];
    
    /* Parse waveform data (bytes 4-13, 10 bytes = 5 words) */
    if (waveform) {
        const uint8_t *wave = &frame_data[4];
        waveform->co2 = parse_uint16_be(&wave[0]) / 100.0f;
        waveform->n2o = parse_uint16_be(&wave[2]) / 100.0f;
        waveform->aa1 = parse_uint16_be(&wave[4]) / 100.0f;
        waveform->aa2 = parse_uint16_be(&wave[6]) / 100.0f;
        waveform->o2 = parse_uint16_be(&wave[8]) / 100.0f;
    }
    
    /* Parse status byte */
    if (status) {
        status->breath_detected = (status_byte & 0x01) != 0;
        status->apnea = (status_byte & 0x02) != 0;
        status->o2_low = (status_byte & 0x04) != 0;
        status->o2_replace = (status_byte & 0x08) != 0;
        status->check_adapter = (status_byte & 0x10) != 0;
        status->accuracy_out_of_range = (status_byte & 0x20) != 0;
        status->sensor_error = (status_byte & 0x40) != 0;
        status->o2_calibration_required = (status_byte & 0x80) != 0;
    }
    
    /* Parse slow data based on frame ID */
    if (slow_data) {
        const uint8_t *slow = &frame_data[14];
        slow_data->last_frame_id = frame_id;
        
        switch (frame_id) {
            case 0x00:
                parse_insp_vals(slow, &slow_data->insp_vals);
                break;
            case 0x01:
                parse_exp_vals(slow, &slow_data->exp_vals);
                break;
            case 0x02:
                parse_mom_vals(slow, &slow_data->mom_vals);
                break;
            case 0x03:
                parse_gen_vals(slow, &slow_data->gen_vals);
                break;
            case 0x04:
                parse_sensor_regs(slow, &slow_data->sensor_regs);
                break;
            case 0x05:
                parse_config_data(slow, &slow_data->config_data);
                break;
            case 0x06:
                parse_service_data(slow, &slow_data->service_data);
                break;
            case 0x07:
            case 0x08:
            case 0x09:
                /* Reserved - no data */
                break;
            default:
                return GAS_SENSOR_ERR_INVALID_FRAME;
        }
    }
    
    return GAS_SENSOR_OK;
}

int gas_sensor_read_frame(gas_sensor_handle_t handle,
                         gas_sensor_slow_data_t *slow_data,
                         gas_sensor_waveform_t *waveform,
                         gas_sensor_status_t *status)
{
    if (!handle) {
        return GAS_SENSOR_ERR_NULL_PARAM;
    }
    
    struct gas_sensor_handle *h = (struct gas_sensor_handle *)handle;
    uint8_t temp_buffer[128];
    int bytes_read = 0;
    
    /* Read available data from serial port */
#ifdef _WIN32
    DWORD dwBytesRead = 0;
    if (!ReadFile(h->serial_port, temp_buffer, sizeof(temp_buffer), &dwBytesRead, NULL)) {
        return GAS_SENSOR_ERR_SERIAL_READ;
    }
    bytes_read = dwBytesRead;
#else
    /* Read with timeout */
    fd_set readfds;
    struct timeval tv;
    
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    
    FD_ZERO(&readfds);
    FD_SET(h->serial_fd, &readfds);
    
    int select_ret = select(h->serial_fd + 1, &readfds, NULL, NULL, &tv);
    if (select_ret <= 0) {
        return GAS_SENSOR_ERR_SERIAL_READ;
    }
    
    bytes_read = read(h->serial_fd, temp_buffer, sizeof(temp_buffer));
    if (bytes_read < 0) {
        return GAS_SENSOR_ERR_SERIAL_READ;
    }
#endif
    
    /* Add new bytes to buffer */
    if (bytes_read > 0) {
        /* Check if buffer would overflow */
        if (h->rx_buffer_len + bytes_read > (int)sizeof(h->rx_buffer)) {
            /* Buffer overflow - reset buffer and start fresh */
            h->rx_buffer_len = 0;
        }
        
        /* Append new data to buffer */
        memcpy(&h->rx_buffer[h->rx_buffer_len], temp_buffer, bytes_read);
        h->rx_buffer_len += bytes_read;
    }
    
    /* Look for frame synchronization (0xAA 0x55) */
    int frame_start = -1;
    for (int i = 0; i < h->rx_buffer_len - 1; i++) {
        if (h->rx_buffer[i] == GAS_SENSOR_FLAG1 && h->rx_buffer[i + 1] == GAS_SENSOR_FLAG2) {
            frame_start = i;
            break;
        }
    }
    
    /* If no frame start found, handle partial buffers */
    if (frame_start == -1) {
        /* If buffer is too small to contain sync pattern, need more data */
        if (h->rx_buffer_len < 2) {
            return GAS_SENSOR_ERR_SERIAL_READ;  /* Need more data */
        }
        /* Discard first byte and try again next time */
        memmove(h->rx_buffer, &h->rx_buffer[1], h->rx_buffer_len - 1);
        h->rx_buffer_len--;
        return GAS_SENSOR_ERR_INVALID_FRAME;
    }
    
    /* Discard any data before frame start */
    if (frame_start > 0) {
        memmove(h->rx_buffer, &h->rx_buffer[frame_start], h->rx_buffer_len - frame_start);
        h->rx_buffer_len -= frame_start;
    }
    
    /* Check if we have a complete frame (21 bytes) */
    if (h->rx_buffer_len < GAS_SENSOR_FRAME_SIZE) {
        return GAS_SENSOR_ERR_SERIAL_READ;  /* Need more data */
    }
    
    /* Parse the frame */
    int result = gas_sensor_parse_frame(h->rx_buffer, &h->slow_data, waveform, status);
    
    /* Remove processed frame from buffer regardless of parse result */
    if (result == GAS_SENSOR_OK || result == GAS_SENSOR_ERR_CHECKSUM) {
        memmove(h->rx_buffer, &h->rx_buffer[GAS_SENSOR_FRAME_SIZE], 
                h->rx_buffer_len - GAS_SENSOR_FRAME_SIZE);
        h->rx_buffer_len -= GAS_SENSOR_FRAME_SIZE;
    }
    
    /* Only proceed if frame parse was successful */
    if (result != GAS_SENSOR_OK) {
        return result;
    }
    
    /* Update slow data pointer if provided */
    if (slow_data) {
        memcpy(slow_data, &h->slow_data, sizeof(gas_sensor_slow_data_t));
    }
    
    /* Call callback if registered */
    if (h->callback) {
        int callback_result = h->callback(&h->slow_data, waveform, status);
        if (callback_result != 0) {
            return GAS_SENSOR_ERR_CALLBACK;
        }
    }
    
    return result;
}

void gas_sensor_init_slow_data(gas_sensor_slow_data_t *slow_data)
{
    if (!slow_data) {
        return;
    }
    
    memset(slow_data, 0, sizeof(gas_sensor_slow_data_t));
    slow_data->last_frame_id = 0xFF;
    
    /* Initialize all concentration fields to invalid */
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
    
    slow_data->gen_vals.resp_rate = GAS_SENSOR_NO_DATA;
    slow_data->gen_vals.time_since_breath = GAS_SENSOR_NO_DATA;
    slow_data->gen_vals.atm_pressure = GAS_SENSOR_CONC_INVALID;
    
    /* Keep other fields at zero (memset did this) */
}

float gas_sensor_parse_concentration(uint8_t raw_value)
{
    return parse_concentration(raw_value);
}

const char *gas_sensor_strerror(int error_code)
{
    switch (error_code) {
        case GAS_SENSOR_OK:
            return "Success";
        case GAS_SENSOR_ERR_INVALID_FRAME:
            return "Invalid frame (bad flags)";
        case GAS_SENSOR_ERR_CHECKSUM:
            return "Checksum verification failed";
        case GAS_SENSOR_ERR_SERIAL_OPEN:
            return "Failed to open serial port";
        case GAS_SENSOR_ERR_SERIAL_READ:
            return "Serial read error";
        case GAS_SENSOR_ERR_SERIAL_WRITE:
            return "Serial write error";
        case GAS_SENSOR_ERR_CALLBACK:
            return "Callback function returned error";
        case GAS_SENSOR_ERR_NULL_PARAM:
            return "NULL parameter provided";
        case GAS_SENSOR_ERR_MEMORY:
            return "Memory allocation failed";
        default:
            return "Unknown error";
    }
}
