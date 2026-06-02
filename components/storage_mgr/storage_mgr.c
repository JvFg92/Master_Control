#include <stdio.h>
#include <string.h>
#include "esp_spiffs.h"
#include "esp_log.h"
#include "storage_mgr.h"

static const char *TAG = "STORAGE_MGR";
#define FILE_PATH "/spiffs/planetas.txt"

void storage_init(void) {
    ESP_LOGI(TAG, "Inicializando SPIFFS...");

    esp_vfs_spiffs_conf_t conf = {
      .base_path = "/spiffs",
      .partition_label = "storage",
      .max_files = 5,
      .format_if_mount_failed = true
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao inicializar SPIFFS (%s)", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "SPIFFS montado com sucesso!");
}

void storage_save_planet(const char *name, const char *uid_str) {
    // Abre o arquivo no modo "append" (adiciona ao final sem apagar o conteúdo antigo)
    FILE* f = fopen(FILE_PATH, "a");
    if (f == NULL) {
        ESP_LOGE(TAG, "Falha ao abrir arquivo para escrita");
        return;
    }
    // Grava no formato: PLANETA,UID
    fprintf(f, "%s,%s\n", name, uid_str);
    fclose(f);
    ESP_LOGI(TAG, "Dados persistidos: %s -> %s", name, uid_str);
}

void storage_get_file_content(char *output_buffer, size_t max_size) {
    FILE* f = fopen(FILE_PATH, "r");
    if (f == NULL) {
        snprintf(output_buffer, max_size, "Nenhum registro encontrado.\n");
        return;
    }

    char line[64];
    size_t current_len = 0;
    output_buffer[0] = '\0';

    // Lê linha por linha e concatena no buffer de saída
    while (fgets(line, sizeof(line), f) != NULL) {
        if (current_len + strlen(line) < max_size - 1) {
            strcat(output_buffer, line);
            current_len += strlen(line);
        } else {
            break; // Buffer cheio
        }
    }
    fclose(f);
}

void storage_clear_all(void) {
    FILE* f = fopen(FILE_PATH, "w");
    if (f != NULL) fclose(f);
    ESP_LOGI(TAG, "Arquivo de registros limpo.");
}