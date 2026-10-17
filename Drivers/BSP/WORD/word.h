#ifndef WORD_H
#define WORD_H

#include "./SYSTEM/sys/sys.h"



void sddata_to_flash();
void data_to_word(uint16_t i);
void flash_to_offset(uint8_t j);
void offset_to_wordlist();
void sram_to_word_list(uint8_t p);
uint32_t find_word();






#endif

