#include "EPD_config.h"

#include <string.h>

#include "nordic_common.h"
#include "nrf.h"
#include "nrf_log.h"
#include "nrf_sdh_soc.h"
#include "nrf_soc.h"

#define CONFIG_PAGE_ADDR 0x26000UL
#define CONFIG_PAGE_SIZE 4096
#define CONFIG_MAGIC     0x45504443UL

typedef struct {
    uint32_t magic;
    uint8_t  data[16];
    uint32_t check;
} config_record_t;

static config_record_t  m_pending;
static volatile uint8_t m_write_state = 0;

static uint32_t record_checksum(const uint8_t* data) {
    uint32_t sum = 0;
    for (uint8_t i = 0; i < sizeof(m_pending.data); i++) sum += data[i];
    return sum;
}

static bool record_valid(const config_record_t* record) {
    return record->magic == CONFIG_MAGIC && record->check == record_checksum(record->data);
}

static void config_soc_evt_handler(uint32_t evt_id, void* p_context) {
    UNUSED_PARAMETER(p_context);

    if (evt_id != NRF_EVT_FLASH_OPERATION_SUCCESS && evt_id != NRF_EVT_FLASH_OPERATION_ERROR) return;

    if (m_write_state == 1) {
        uint32_t ret = sd_flash_write((uint32_t*)CONFIG_PAGE_ADDR, (uint32_t*)&m_pending,
                                      sizeof(m_pending) / sizeof(uint32_t));
        m_write_state = (ret == NRF_SUCCESS) ? 2 : 0;
    } else if (m_write_state == 2) {
        m_write_state = 0;
    }
}

NRF_SDH_SOC_OBSERVER(m_config_soc_obs, 0, config_soc_evt_handler, NULL);

void epd_config_init(epd_config_t* cfg) { memset(cfg, 0xFF, sizeof(epd_config_t)); }

void epd_config_read(epd_config_t* cfg) {
    const config_record_t* record = (const config_record_t*)CONFIG_PAGE_ADDR;

    memset(cfg, 0xFF, sizeof(epd_config_t));
    if (record_valid(record)) memcpy(cfg, record->data, EPD_CONFIG_SIZE);
}

void epd_config_write(epd_config_t* cfg) {
    if (m_write_state != 0) return;

    memset(&m_pending, 0xFF, sizeof(m_pending));
    m_pending.magic = CONFIG_MAGIC;
    memcpy(m_pending.data, cfg, EPD_CONFIG_SIZE);
    m_pending.check = record_checksum(m_pending.data);

    m_write_state = 1;
    uint32_t ret = sd_flash_page_erase(CONFIG_PAGE_ADDR / CONFIG_PAGE_SIZE);
    if (ret != NRF_SUCCESS) m_write_state = 0;
}

void epd_config_clear(epd_config_t* cfg) {
    static const uint32_t invalid = 0;

    memset(cfg, 0xFF, sizeof(epd_config_t));
    if (m_write_state != 0) return;

    m_write_state = 2;
    uint32_t ret = sd_flash_write((uint32_t*)CONFIG_PAGE_ADDR, (uint32_t*)&invalid, 1);
    if (ret != NRF_SUCCESS) m_write_state = 0;
}

bool epd_config_empty(epd_config_t* cfg) {
    for (uint8_t i = 0; i < EPD_CONFIG_SIZE; i++) {
        if (((uint8_t*)cfg)[i] != 0xFF) return false;
    }
    return true;
}
