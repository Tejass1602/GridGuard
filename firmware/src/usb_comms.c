#include "gg_app.h"
#include "gg_protocol.h"
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/usb/usbd.h>
#include <zephyr/sys/ring_buffer.h>
#include <zephyr/kernel.h>
#include <string.h>

static const struct device *const uart = DEVICE_DT_GET(DT_NODELABEL(cdc_acm_uart0));

USBD_DEVICE_DEFINE(gg_usbd, DEVICE_DT_GET(DT_NODELABEL(zephyr_udc0)), 0x2fe3, 0x0001);
USBD_DESC_LANG_DEFINE(gg_lang);
USBD_DESC_MANUFACTURER_DEFINE(gg_mfr, "GridGuard");
USBD_DESC_PRODUCT_DEFINE(gg_prod, "GridGuard Pico 2 W");
USBD_DESC_CONFIG_DEFINE(gg_fs_cfg_desc, "FS Configuration");
USBD_CONFIGURATION_DEFINE(gg_fs_config, USB_SCD_SELF_POWERED, 125, &gg_fs_cfg_desc);

RING_BUF_DECLARE(rx_ring, 512);
RING_BUF_DECLARE(tx_ring, 2048);
static uint8_t rx_encoded[GG_FRAME_MAX + 4], rx_payload[GG_FRAME_MAX];
static size_t rx_used;
static uint32_t crc_errors;
static unsigned tx_ticks;
K_MUTEX_DEFINE(tx_lock);

static void cdc_irq_handler(const struct device *dev, void *user_data) {
    ARG_UNUSED(user_data);
    while (uart_irq_update(dev) && uart_irq_is_pending(dev)) {
        if (uart_irq_rx_ready(dev)) {
            uint8_t buf[64];
            int n = uart_fifo_read(dev, buf, sizeof(buf));
            if (n > 0) {
                ring_buf_put(&rx_ring, buf, (uint32_t)n);
            }
        }
        if (uart_irq_tx_ready(dev)) {
            uint8_t buf[64];
            uint32_t len = ring_buf_get(&tx_ring, buf, sizeof(buf));
            if (len > 0) {
                (void)uart_fifo_fill(dev, buf, (int)len);
            } else {
                uart_irq_tx_disable(dev);
            }
        }
    }
}

static void send_frame(uint8_t type, const void *data, uint16_t len) {
    uint8_t wire[GG_FRAME_MAX + 8];
    size_t n;
    uint32_t dtr = 0; if (!device_is_ready(uart)) return; uart_line_ctrl_get(uart, UART_LINE_CTRL_DTR, &dtr); if (!dtr) return;
    k_mutex_lock(&tx_lock, K_FOREVER);
    n = gg_frame_encode(type, gg_app.seq++, (const uint8_t *)data, len, wire, sizeof(wire));
    if (ring_buf_space_get(&tx_ring) < n) ring_buf_reset(&tx_ring);
    ring_buf_put(&tx_ring, wire, (uint32_t)n);
    uart_irq_tx_enable(uart);
    k_mutex_unlock(&tx_lock);
}

static uint16_t read16(const uint8_t *p) { return (uint16_t)(p[0] | ((uint16_t)p[1] << 8)); }
static float readf(const uint8_t *p) { float x; memcpy(&x, p, sizeof(x)); return x; }

static void ack(const gg_frame_header_t *h, uint8_t status) {
    uint8_t p[4] = {(uint8_t)h->seq, (uint8_t)(h->seq >> 8), h->msg_type, status};
    send_frame(0x15, p, sizeof(p));
}

static void handle_command(const gg_frame_header_t *h, const uint8_t *p) {
    uint8_t status = 0;
    switch (h->msg_type) {
    case 0x10:
        if (h->payload_len != 2 || p[0] >= 4 || p[1] > 1) status = 2;
        else gg_plant_set_appliance(&gg_app.plant, p[0], p[1]);
        break;
    case 0x11:
        if (h->payload_len != 5 || p[0] > 7) status = 2;
        else gg_plant_set_fault(&gg_app.plant, (gg_fault_scenario_t)p[0], readf(p + 1));
        break;
    case 0x12: {
        float src = 0, load = 0;
        int aux = 0;
        uint8_t force = h->payload_len == 1 ? p[0] : 2;
        if (force > 1) status = 2;
        else {
            if (gg_app.feedback_mode <= 1) (void)gg_hal_feedback(&src, &load, &aux);
            else src = gg_app.feedback_mode == FEEDBACK_VIRTUAL_NO_SRC ? 0 : 12;
            if (force && gg_app.fault != FAULT_NONE) {
                gg_app.fault = FAULT_NONE;
                gg_app.protection.overcurrent_count = 0;
                gg_app.protection.thermal = 0;
            }
            if (!gg_relay_request_reset(&gg_app.relay, gg_app.fault == FAULT_NONE, src)) status = 1;
        }
        break;
    }
    case 0x13:
        if (h->payload_len != 1 || p[0] > 4) status = 2;
        else gg_app.feedback_mode = p[0];
        break;
    case 0x14:
        if (h->payload_len != 4) status = 2;
        else gg_store_send_faults(read16(p), read16(p + 2));
        break;
    default:
        status = 2;
        break;
    }
    ack(h, status);
}

static void receive_byte(uint8_t byte) {
    gg_frame_header_t h;
    int rc;
    if (byte) {
        if (rx_used < sizeof(rx_encoded)) rx_encoded[rx_used++] = byte;
        else rx_used = 0;
        return;
    }
    if (!rx_used) return;
    rc = gg_frame_decode(rx_encoded, rx_used, &h, rx_payload, sizeof(rx_payload));
    rx_used = 0;
    if (rc) { crc_errors++; return; }
    if (h.msg_type >= 0x10 && h.msg_type <= 0x14) handle_command(&h, rx_payload);
}

int gg_comms_init(void) {
    int rc;
    if (!device_is_ready(uart)) return -ENODEV;
    rc = usbd_add_descriptor(&gg_usbd, &gg_lang);
    if (rc) return rc;
    rc = usbd_add_descriptor(&gg_usbd, &gg_mfr);
    if (rc) return rc;
    rc = usbd_add_descriptor(&gg_usbd, &gg_prod);
    if (rc) return rc;
    rc = usbd_add_configuration(&gg_usbd, USBD_SPEED_FS, &gg_fs_config);
    if (rc) return rc;
    rc = usbd_register_all_classes(&gg_usbd, USBD_SPEED_FS, 1, NULL);
    if (rc) return rc;
    usbd_device_set_code_triple(&gg_usbd, USBD_SPEED_FS, USB_BCC_MISCELLANEOUS, 0x02, 0x01);
    rc = usbd_init(&gg_usbd);
    if (rc) return rc;
    rc = usbd_enable(&gg_usbd);
    if (rc) return rc;
    uart_irq_callback_user_data_set(uart, cdc_irq_handler, NULL);
    uart_irq_rx_enable(uart);
    return 0;
}

void gg_comms_rx_task(void) {
    uint8_t b;
    while (ring_buf_get(&rx_ring, &b, 1) == 1) receive_byte(b);
}

void gg_comms_send_telemetry(void) {
    gg_msg_telemetry_t t;
    unsigned i;
    memset(&t, 0, sizeof(t));
    t.ts_ms = gg_app.timestamp_ms;
    t.v_rms = gg_app.metrics.v_rms;
    t.i_rms = gg_app.metrics.i_rms;
    t.p_w = gg_app.metrics.p_w;
    t.q1_var = gg_app.metrics.q1_var;
    t.s_va = gg_app.metrics.s_va;
    t.pf = gg_app.metrics.pf;
    t.freq_hz = gg_app.metrics.freq_hz;
    t.thd_i_pct = gg_app.metrics.thd_i_pct;
    t.thd_v_pct = gg_app.metrics.thd_v_pct;
    t.energy_wh = gg_app.metrics.energy_wh;
    t.thermal_pct = gg_app.protection.thermal * 100.0f;
    for (i = 0; i < 5; i++) t.harm_i_rms[i] = gg_app.metrics.harm_i_rms[i];
    send_frame(0x02, &t, sizeof(t));
    if (++tx_ticks % 4 == 0) { gg_comms_send_protection(); gg_comms_send_waveform(); }
    if (tx_ticks % 10 == 0) gg_comms_send_nilm();
    if (tx_ticks % 20 == 0) gg_comms_send_health();
    if (tx_ticks % 40 == 0) gg_comms_send_hello();
}

void gg_comms_send_protection(void) {
    gg_msg_protection_t p;
    float src = 0, load = 0;
    int aux = 0;
    memset(&p, 0, sizeof(p));
    if (gg_app.feedback_mode <= 1) (void)gg_hal_feedback(&src, &load, &aux);
    else {
        src = gg_app.feedback_mode == FEEDBACK_VIRTUAL_NO_SRC ? 0 : 12;
        load = gg_app.feedback_mode == FEEDBACK_VIRTUAL_WELDED ? 12 : (gg_app.relay.coil_cmd ? 12 : 0);
    }
    p.ts_ms = k_uptime_get_32();
    p.relay_state = (uint8_t)gg_app.relay.state;
    p.active_fault = (uint8_t)gg_app.fault;
    p.coil_cmd = (uint8_t)gg_app.relay.coil_cmd;
    p.aux_state = (uint8_t)aux;
    p.feedback_mode = gg_app.feedback_mode;
    p.verify_samples = (uint8_t)gg_app.relay.samples;
    p.v_src_v = src;
    p.v_load_v = load;
    p.trip_latency_us = gg_app.trip_latency_us;
    send_frame(0x05, &p, sizeof(p));
}

void gg_comms_send_waveform(void) {
    gg_msg_waveform_t w;
    unsigned i;
    w.ts_ms = gg_app.timestamp_ms;
    for (i = 0; i < 64; i++) {
        w.v_samples[i] = (int16_t)(((int)gg_app.waveform[i].voltage_code - 2048) * 4000 / 2047);
        w.i_samples[i] = (int16_t)(((int)gg_app.waveform[i].current_code - 2048) * 4000 / 2047);
    }
    send_frame(0x03, &w, sizeof(w));
}

void gg_comms_send_nilm(void) {
    gg_msg_nilm_state_t n;
    unsigned i;
    memset(&n, 0, sizeof(n));
    n.ts_ms = gg_app.timestamp_ms;
    n.last_event_id = gg_app.nilm.last_event_id;
    n.last_event_type = gg_app.nilm.last_event_type;
    for (i = 0; i < 4; i++) {
        if (gg_app.nilm.state[i]) n.active_mask |= (uint8_t)(1u << i);
        n.est_power_w[i] = gg_app.nilm.est_power_w[i];
        n.confidence[i] = gg_app.nilm.confidence[i];
    }
    n.unknown_power_w = gg_app.nilm.unknown_power_w;
    n.last_mahalanobis = gg_app.nilm.mahalanobis;
    send_frame(0x04, &n, sizeof(n));
}

void gg_comms_send_health(void) {
    gg_msg_health_t h;
    size_t unused = 0;
    memset(&h, 0, sizeof(h));
    h.ts_ms = k_uptime_get_32();
    h.dsp_exec_us = gg_app.dsp_exec_us;
    h.max_dsp_us = gg_app.max_dsp_us;
    h.dropped_frames = gg_app.dropped_frames;
    h.uart_crc_err = crc_errors;
    h.nvs_records = gg_store_record_count();
    if (k_thread_stack_space_get(k_current_get(), &unused) == 0) h.free_stack_min = (uint16_t)MIN(unused, UINT16_MAX);
    send_frame(0x07, &h, sizeof(h));
}

void gg_comms_send_fault_record(uint32_t id, uint32_t ts, uint8_t fault, uint8_t result, uint8_t coil, uint8_t aux, float trigger, float threshold, float source, float load, uint16_t verify, uint32_t crc) {
    gg_msg_fault_record_t r = {id, ts, fault, result, coil, aux, trigger, threshold, source, load, verify, crc};
    send_frame(0x06, &r, sizeof(r));
}

void gg_comms_send_hello(void) {
    gg_msg_hello_t hello = {1, 0, 2, gg_app.feedback_mode, k_uptime_get_32()};
    send_frame(0x01, &hello, sizeof(hello));
}
