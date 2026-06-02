#ifndef NFC_READER_H
#define NFC_READER_H

#include <stdint.h>
#include <stdbool.h>

// Definições de blocos comuns para gravação (Mifare Classic 1K)
// Evite blocos como 0 (Dados do fabricante) e blocos múltiplos de 4 finais (Trailers de setor)
#define PLANET_DATA_BLOCK  4  

// Funções públicas do módulo
void nfc_reader_init(void);
void nfc_reader_start_reading(void);
void nfc_reader_stop_reading(void);
bool nfc_reader_write_data(uint8_t block, uint8_t *data_16_bytes);

#endif // NFC_READER_H