#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/flash.h" // for the flash erasing and writing
#include <hardware/sync.h>
#include "persist.h"

#define FLASH_TARGET_OFFSET (512 * 1024) // choosing to start at 512K
#define SIG_SIZE 5
typedef struct {
    char signature[SIG_SIZE];
    KEY_STRING  tbl[6];
} MY_MEMORY;
MY_MEMORY my_memory;
const char DATA_SIGNATURE[] = "MAGIC";

#define MEMORY_SIZE sizeof(my_memory)

KEY_STRING* persist_readBackMyData() {
    const uint8_t* flash_target_contents = (const uint8_t *) (XIP_BASE + FLASH_TARGET_OFFSET);
    if( memcmp(DATA_SIGNATURE, flash_target_contents, SIG_SIZE) == 0) {
        memcpy(&my_memory, flash_target_contents, MEMORY_SIZE);
    }
    else {
        // init empty data
        memset(&my_memory,0, MEMORY_SIZE);
        memcpy( my_memory.signature, DATA_SIGNATURE, SIG_SIZE );
    }
    return my_memory.tbl;
}

void persist_saveMyData() {
    uint8_t* myDataAsBytes = (uint8_t*) &my_memory;
    int myDataSize = MEMORY_SIZE;
    
    int writeSize = (myDataSize / FLASH_PAGE_SIZE) + 1; // how many flash pages we're gonna need to write
    int sectorCount = ((writeSize * FLASH_PAGE_SIZE) / FLASH_SECTOR_SIZE) + 1; // how many flash sectors we're gonna need to erase
        
    printf("Programming flash target region...\n");

    uint32_t interrupts = save_and_disable_interrupts();
    flash_range_erase(FLASH_TARGET_OFFSET, FLASH_SECTOR_SIZE * sectorCount);
    flash_range_program(FLASH_TARGET_OFFSET, myDataAsBytes, FLASH_PAGE_SIZE * writeSize);
    restore_interrupts(interrupts);

    printf("Done.\n");
}