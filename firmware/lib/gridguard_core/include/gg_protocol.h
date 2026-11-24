#ifndef GG_PROTOCOL_H
#define GG_PROTOCOL_H
#include <stddef.h>
#include <stdint.h>
#define GG_PROTOCOL_VERSION 1
#define GG_FRAME_MAX 512
#define GG_HEADER_SIZE 6
#define GG_CRC_SIZE 2
#if defined(__GNUC__)
#define GG_PACKED __attribute__((packed))
#else
#define GG_PACKED
#endif
typedef struct GG_PACKED { uint8_t version, msg_type; uint16_t seq, payload_len; } gg_frame_header_t;
typedef struct GG_PACKED { uint8_t fw_major, fw_minor, board_id, feedback_mode; uint32_t uptime_ms; } gg_msg_hello_t;
typedef struct GG_PACKED { uint32_t ts_ms; float v_rms,i_rms,p_w,q1_var,s_va,pf,freq_hz,thd_i_pct,thd_v_pct,energy_wh,thermal_pct,harm_i_rms[5]; } gg_msg_telemetry_t;
typedef struct GG_PACKED { uint32_t ts_ms; int16_t v_samples[64], i_samples[64]; } gg_msg_waveform_t;
typedef struct GG_PACKED { uint32_t ts_ms; uint8_t active_mask,last_event_id,last_event_type,reserved; float est_power_w[4],confidence[4],unknown_power_w,last_mahalanobis; uint32_t inference_us; } gg_msg_nilm_state_t;
typedef struct GG_PACKED { uint32_t ts_ms; uint8_t relay_state,active_fault,coil_cmd,aux_state,feedback_mode,verify_samples; uint16_t reserved; float v_src_v,v_load_v; uint32_t trip_latency_us; } gg_msg_protection_t;
typedef struct GG_PACKED { uint32_t record_id,ts_ms; uint8_t fault_code,relay_result,coil_cmd,aux_state; float trigger_val,threshold_val,v_src_v,v_load_v; uint16_t verify_ms; uint32_t crc32; } gg_msg_fault_record_t;
typedef struct GG_PACKED { uint32_t ts_ms,dsp_exec_us,max_dsp_us,dropped_frames,uart_crc_err; uint16_t nvs_records,free_stack_min; } gg_msg_health_t;
uint16_t gg_crc16_ccitt(const uint8_t *data, size_t len);
size_t gg_cobs_encode(const uint8_t *input,size_t length,uint8_t *output,size_t capacity);
size_t gg_cobs_decode(const uint8_t *input,size_t length,uint8_t *output,size_t capacity);
size_t gg_frame_encode(uint8_t type,uint16_t seq,const uint8_t *payload,uint16_t payload_len,uint8_t *wire,size_t capacity);
int gg_frame_decode(const uint8_t *wire,size_t wire_len,gg_frame_header_t *header,uint8_t *payload,size_t capacity);
#endif
