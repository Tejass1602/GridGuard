#include "gg_app.h"
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/fs/nvs.h>
#include <zephyr/sys/crc.h>
#include <stddef.h>
#include <string.h>

#define GG_META_KEY 0x0001
#define GG_LOG_BASE 0x1000
struct __packed fault_record {uint32_t record_id,ts_ms;uint8_t fault_code,relay_result,coil_cmd,aux_state;float trigger,threshold,source,load;uint16_t verify_ms;uint32_t crc32;uint8_t reserved[14];};
BUILD_ASSERT(sizeof(struct fault_record)==48,"fault record wire/storage size must stay 48 bytes");
static struct nvs_fs fs;
static uint32_t record_count;
static bool mounted;
static bool recovered_fault;
static bool record_valid(const struct fault_record *rec){return rec->record_id!=0&&rec->crc32==crc32_ieee((const uint8_t *)rec,offsetof(struct fault_record,crc32));}
int gg_store_init(void){int rc;struct flash_pages_info page;uint32_t latest=0;int i;
 fs.flash_device=FIXED_PARTITION_DEVICE(storage_partition);fs.offset=FIXED_PARTITION_OFFSET(storage_partition);
 if(!device_is_ready(fs.flash_device))return -ENODEV;
 rc=flash_get_page_info_by_offs(fs.flash_device,fs.offset,&page);if(rc)return rc;
 fs.sector_size=page.size;fs.sector_count=0x10000u/fs.sector_size;
 rc=nvs_mount(&fs);if(rc)return rc;mounted=true;
 if(nvs_read(&fs,GG_META_KEY,&record_count,sizeof(record_count))!=(ssize_t)sizeof(record_count))record_count=0;
 for(i=0;i<64;i++){struct fault_record rec;ssize_t n=nvs_read(&fs,GG_LOG_BASE+i,&rec,sizeof(rec));if(n==(ssize_t)sizeof(rec)&&record_valid(&rec)&&rec.record_id>latest)latest=rec.record_id;}
 if(latest>record_count)record_count=latest;
 if(record_count){struct fault_record latest_rec;uint16_t key=(uint16_t)(GG_LOG_BASE+(record_count-1)%64);if(nvs_read(&fs,key,&latest_rec,sizeof(latest_rec))==(ssize_t)sizeof(latest_rec)&&record_valid(&latest_rec)&&(latest_rec.relay_result==TRIP_FAILURE||latest_rec.fault_code!=FAULT_NONE)){gg_app.relay.state=BOOT_SAFE_OPEN;gg_app.relay.coil_cmd=0;gg_app.fault=(gg_fault_code_t)latest_rec.fault_code;recovered_fault=true;}}
 return 0;
}
int gg_store_has_latched_fault(void){return recovered_fault;}
uint16_t gg_store_record_count(void){return (uint16_t)MIN(record_count,65535);}
int gg_store_fault(gg_fault_code_t fault,gg_relay_state_t result,float trigger,float threshold,float source,float load,uint32_t ts){struct fault_record rec;uint16_t key;ssize_t n;
 if(!mounted)return -ENODEV;
 memset(&rec,0,sizeof(rec));rec.record_id=++record_count;rec.ts_ms=ts;rec.fault_code=(uint8_t)fault;rec.relay_result=(uint8_t)result;rec.coil_cmd=(uint8_t)gg_app.relay.coil_cmd;rec.aux_state=0;rec.trigger=trigger;rec.threshold=threshold;rec.source=source;rec.load=load;rec.verify_ms=(uint16_t)MIN(gg_app.relay.elapsed_ms,65535);rec.crc32=crc32_ieee((const uint8_t *)&rec,offsetof(struct fault_record,crc32));
 key=(uint16_t)(GG_LOG_BASE+(rec.record_id-1)%64);n=nvs_write(&fs,key,&rec,sizeof(rec));if(n<0)return (int)n;n=nvs_write(&fs,GG_META_KEY,&record_count,sizeof(record_count));if(n<0)return (int)n;gg_comms_send_fault_record(rec.record_id,rec.ts_ms,rec.fault_code,rec.relay_result,rec.coil_cmd,rec.aux_state,rec.trigger,rec.threshold,rec.source,rec.load,rec.verify_ms,rec.crc32);return 0;
}
void gg_store_send_faults(uint16_t start,uint16_t count){uint32_t first,last,id;if(!mounted||count==0)return;first=(uint32_t)start;if(first>=record_count)return;last=MIN(record_count,first+MIN(count,64));for(id=first;id<last;id++){struct fault_record rec;uint16_t key=(uint16_t)(GG_LOG_BASE+id%64);if(nvs_read(&fs,key,&rec,sizeof(rec))==(ssize_t)sizeof(rec)&&record_valid(&rec))gg_comms_send_fault_record(rec.record_id,rec.ts_ms,rec.fault_code,rec.relay_result,rec.coil_cmd,rec.aux_state,rec.trigger,rec.threshold,rec.source,rec.load,rec.verify_ms,rec.crc32);}}
