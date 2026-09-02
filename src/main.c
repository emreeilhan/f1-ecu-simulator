#include <stdio.h>
#include "ecu.h"
#include <stdint.h>

int main(void) {
    EcuState ecu = {0};
    const uint8_t frame[] = {0x0B, 0xB8};

    if(!ecu_update_rpm(&ecu,frame,sizeof frame)){
        fprintf(stderr,"RPM Frame could not be decoded\n");
        return 1;
    }
    printf("RPM: %u\n", (unsigned int)ecu.rpm);
    return 0;
}