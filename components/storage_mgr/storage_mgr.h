#ifndef STORAGE_MGR_H
#define STORAGE_MGR_H

void storage_init(void);
void storage_save_planet(const char *name, const char *uid_str);
void storage_get_file_content(char *output_buffer, size_t max_size);
void storage_clear_all(void);

#endif