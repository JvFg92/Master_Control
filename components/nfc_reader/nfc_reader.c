#include <stdio.h>
#include <string.h>
#include "nfc_reader.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Pinagem estabelecida
#define PIN_NUM_MISO 19
#define PIN_NUM_MOSI 23
#define PIN_NUM_CLK  18
#define PIN_NUM_CS   5
#define PIN_NUM_RST  22

// Registradores e Comandos do RC522 / Mifare
#define CommandReg    0x01
#define FIFODataReg   0x09
#define FIFOLevelReg  0x0A
#define BitFramingReg 0x0D
#define ModeReg       0x11
#define TxControlReg  0x14
#define TModeReg      0x2A
#define TPrescalerReg 0x2B
#define TReloadRegH   0x2C
#define TReloadRegL   0x2D

#define PCD_RESETPHASE   0x0F
#define PCD_TRANSCEIVE   0x0C
#define PCD_AUTHENT      0x0E
#define PICC_REQIDL      0x26
#define PICC_ANTICOLL    0x93
#define PICC_WRITE       0xA0

// ---------------- Extern Declaration ----------------
extern void handle_nfc_detection(uint8_t *uid);

static const char *TAG = "NFC_READER";
static spi_device_handle_t spi_handle;
static TaskHandle_t nfc_task_handle = NULL;

// Chave de autenticação padrão de fábrica (Mifare A: FF FF FF FF FF FF)
static const uint8_t FACTORY_KEY[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// --- Funções de Baixo Nível de Comunicação SPI ---
static void rc522_write_reg(uint8_t reg, uint8_t val) {
    uint8_t tx_data[2] = { (reg << 1) & 0x7E, val };
    spi_transaction_t t = { .length = 16, .tx_buffer = tx_data };
    spi_device_polling_transmit(spi_handle, &t);
}

static uint8_t rc522_read_reg(uint8_t reg) {
    uint8_t tx_data[2] = { ((reg << 1) & 0x7E) | 0x80, 0 };
    uint8_t rx_data[2] = {0};
    spi_transaction_t t = { .length = 16, .tx_buffer = tx_data, .rx_buffer = rx_data };
    spi_device_polling_transmit(spi_handle, &t);
    return rx_data[1];
}

static void rc522_set_bit_mask(uint8_t reg, uint8_t mask) {
    rc522_write_reg(reg, rc522_read_reg(reg) | mask);
}

static void rc522_clear_bit_mask(uint8_t reg, uint8_t mask) {
    rc522_write_reg(reg, rc522_read_reg(reg) & (~mask));
}

// --- Comunicação Direta com o Chip (Transceive) ---
static esp_err_t rc522_to_card(uint8_t command, uint8_t *send_data, uint8_t send_len, uint8_t *back_data, uint32_t *back_len) {
    rc522_write_reg(CommandReg, 0x00); // Idle
    rc522_write_reg(0x04, 0x7F);       // Limpa interrupções
    rc522_set_bit_mask(FIFOLevelReg, 0x80); // Limpa FIFO

    for (int i = 0; i < send_len; i++) {
        rc522_write_reg(FIFODataReg, send_data[i]);
    }

    rc522_write_reg(CommandReg, command);
    if (command == PCD_TRANSCEIVE) {
        rc522_set_bit_mask(BitFramingReg, 0x80); // Inicia transmissão
    }

    // Aguarda resposta por até 25ms
    int timeout = 2500;
    uint8_t n;
    do {
        n = rc522_read_reg(0x04);
        timeout--;
        esp_rom_delay_us(10);
    } while (timeout > 0 && !(n & 0x30));

    rc522_clear_bit_mask(BitFramingReg, 0x80);

    if (timeout == 0) return ESP_ERR_TIMEOUT;

    if (!(rc522_read_reg(0x06) & 0x1B)) { // Verifica erros de colisão/paridade
        if (back_data && back_len) {
            n = rc522_read_reg(FIFOLevelReg);
            if (n > *back_len) n = *back_len;
            *back_len = n;
            for (int i = 0; i < n; i++) {
                back_data[i] = rc522_read_reg(FIFODataReg);
            }
        }
        return ESP_OK;
    }
    return ESP_FAIL;
}

// --- Funções de Varredura e Busca do Cartão ---
static esp_err_t rc522_request(uint8_t req_mode, uint8_t *tag_type) {
    uint32_t len = 2;
    rc522_write_reg(BitFramingReg, 0x07); // Ajusta bits de sincronia
    return rc522_to_card(PCD_TRANSCEIVE, &req_mode, 1, tag_type, &len);
}

static esp_err_t rc522_anticoll(uint8_t *uid) {
    uint32_t len = 5;
    uint8_t cmd[2] = { PICC_ANTICOLL, 0x20 };
    rc522_write_reg(BitFramingReg, 0x00);
    return rc522_to_card(PCD_TRANSCEIVE, cmd, 2, uid, &len);
}

// --- Inicialização do Hardware ---
void nfc_reader_init(void) {
    ESP_LOGI(TAG, "Configurando hardware do barramento SPI...");

    // Inicialização do pino de Reset físico
    gpio_reset_pin(PIN_NUM_RST);
    gpio_set_direction(PIN_NUM_RST, GPIO_MODE_OUTPUT);
    gpio_set_level(PIN_NUM_RST, 1);

    spi_bus_config_t buscfg = {
        .miso_io_num = PIN_NUM_MISO,
        .mosi_io_num = PIN_NUM_MOSI,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 0
    };
    
    // SPI3_HOST representa o hardware VSPI nativo do ESP32
    ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO));

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 1000000, // 1 MHz seguro para prototipagem rápida
        .mode = 0,
        .spics_io_num = PIN_NUM_CS,
        .queue_size = 7,
    };
    ESP_ERROR_CHECK(spi_bus_add_device(SPI3_HOST, &devcfg, &spi_handle));

    // Hard Reset e calibração interna do RC522
    gpio_set_level(PIN_NUM_RST, 0);
    esp_rom_delay_us(10);
    gpio_set_level(PIN_NUM_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(50));

    rc522_write_reg(CommandReg, PCD_RESETPHASE);
    vTaskDelay(pdMS_TO_TICKS(10));
    
    // Configura os Timers internos do chip para timeout de recepção RF
    rc522_write_reg(TModeReg, 0x8D);
    rc522_write_reg(TPrescalerReg, 0x3E);
    rc522_write_reg(TReloadRegH, 0x00);
    rc522_write_reg(TReloadRegL, 0x1E);
    rc522_write_reg(ModeReg, 0x3D);

    rc522_write_reg(0x26, 0x70); // RFCfgReg = 0x70 (Ganho máximo de 48 dB)
    rc522_set_bit_mask(TxControlReg, 0x03);
    // -------------------------------------------------------------------
    // ADICIONE ESTA LINHA DE DIAGNÓSTICO AQUI:
    uint8_t versao = rc522_read_reg(0x37);
    printf("\n[DIAGNOSTICO SPI] Versao do Firmware do RC522: 0x%02X\n", versao);
    if (versao == 0x00 || versao == 0xFF) {
        printf("[ERRO CRITICO] O ESP32 NAO esta conseguindo conversar com o RC522. Verifique os fios!\n\n");
    } else {
        printf("[SUCESSO] Comunicacao SPI com o RC522 operando perfeitamente.\n\n");
    }
    // -------------------------------------------------------------------

    ESP_LOGI(TAG, "Módulo RC522 Pronto e Antena Ligada!");    rc522_set_bit_mask(TxControlReg, 0x03);
    ESP_LOGI(TAG, "Módulo RC522 Pronto e Antena Ligada!");
}

// --- Loop assíncrono em Background (FreeRTOS) ---
static void nfc_reading_task(void *pvParameters) {
    uint8_t tag_type[2];
    uint8_t uid[5];

    ESP_LOGI(TAG, "Tarefa de leitura NFC iniciada.");
    while (1) {
        //printf("."); // Imprime um ponto para provar que está lendo
        fflush(stdout);

        // 0x52 = PICC_REQALL (Acorda qualquer tag)
        if (rc522_request(0x52, tag_type) == ESP_OK) { 
            if (rc522_anticoll(uid) == ESP_OK) {
                handle_nfc_detection(uid);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(300)); 
    }
}

// --- Funções de Controle da Operação de Leitura ---
void nfc_reader_start_reading(void) {
    if (nfc_task_handle == NULL) {
        xTaskCreate(nfc_reading_task, "nfc_reading_task", 4096, NULL, 4, &nfc_task_handle);
        ESP_LOGI(TAG, "Leitura Contínua Ativada.");
    }
}

void nfc_reader_stop_reading(void) {
    if (nfc_task_handle != NULL) {
        vTaskDelete(nfc_task_handle);
        nfc_task_handle = NULL;
        ESP_LOGI(TAG, "Leitura Contínua Desativada com sucesso.");
    }
}

// --- Função de Gravação de Dados na Tag ---
bool nfc_reader_write_data(uint8_t block, uint8_t *data_16_bytes) {
    uint8_t uid[5];
    uint8_t tag_type[2];
    
    // Para gravar, o cartão precisa estar presente e selecionado no exato instante
    if (rc522_request(PICC_REQIDL, tag_type) != ESP_OK || rc522_anticoll(uid) != ESP_OK) {
        ESP_LOGE(TAG, "Gravação abortada: Nenhuma tag posicionada no leitor.");
        return false;
    }

    // Monta pacote estruturado de Autenticação Mifare Crypto1
    uint8_t auth_cmd[12];
    auth_cmd[0] = 0x60; // Comando de Autenticação usando Chave A
    auth_cmd[1] = block;
    memcpy(&auth_cmd[2], FACTORY_KEY, 6);
    memcpy(&auth_cmd[8], uid, 4);

    uint32_t back_len = 0;
    if (rc522_to_card(PCD_AUTHENT, auth_cmd, 12, NULL, &back_len) != ESP_OK) {
        ESP_LOGE(TAG, "Erro de Autenticação no Bloco %d. Chave inválida.", block);
        return false;
    }

    // Prepara a escrita física dos 16 bytes solicitados
    uint8_t write_cmd[2] = { PICC_WRITE, block };
    if (rc522_to_card(PCD_TRANSCEIVE, write_cmd, 2, NULL, &back_len) != ESP_OK) {
        return false;
    }

    if (rc522_to_card(PCD_TRANSCEIVE, data_16_bytes, 16, NULL, &back_len) == ESP_OK) {
        ESP_LOGI(TAG, "Dados gravados com sucesso no bloco %d!", block);
        return true;
    }

    ESP_LOGE(TAG, "Falha na transmissão da gravação física.");
    return false;
}