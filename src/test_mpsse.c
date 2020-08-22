
// a tiny utility to scan a JTAG chain using MPSSE from OpenOCD

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "jtag/drivers/mpsse.h"

#define JTAG_MODE (LSB_FIRST | POS_EDGE_IN | NEG_EDGE_OUT)

uint8_t arr_tdi[128] = {0};
uint8_t arr_tms[128] = {0};
uint8_t arr_tdo[128] = {0};
unsigned int cur_pos = 0;

void clock_raw(int tms, int tdi, int len) {
    for (int i = 0; i < len; i++) {
        unsigned int byte_sel = (cur_pos + i) / 8;
        unsigned int bit_sel = (cur_pos + i) % 8;
        arr_tdi[byte_sel] |= (tdi << bit_sel);
        arr_tms[byte_sel] |= (tms << bit_sel);
    }

    cur_pos += len;
}

void clear() {
    cur_pos = 0;
    memset(arr_tdi, 0, sizeof(arr_tdi));
    memset(arr_tdo, 0, sizeof(arr_tdo));
    memset(arr_tms, 0, sizeof(arr_tms));
}

int main() {
    // FTDI 232H
    const uint16_t vid = 0x0403;
    const uint16_t pid = 0x6014;

    struct mpsse_ctx *ctx = mpsse_open(&vid, &pid, NULL, NULL, NULL, 0);
    printf("ctx = %p\n", ctx);

    if (ctx == NULL) {
        printf("error opening FTDI device\n");
        return EXIT_FAILURE;
    }

    // repeat the procedure from Quartus
    clock_raw(1, 1, 6);
    clock_raw(0, 1, 1);
    clock_raw(1, 1, 1);
    clock_raw(0, 1, 1);
    clock_raw(0, 1, 1);
    clock_raw(0, 1, 1);
    clock_raw(0, 1, 1);
    clock_raw(0, 1, 1);
    clock_raw(0, 1, 1);
    clock_raw(0, 1, 1);
    clock_raw(0, 1, 1);
    clock_raw(0, 1, 1);
    clock_raw(0, 1, 1);
    clock_raw(0, 0, 1);
    clock_raw(0, 0, 1);
    clock_raw(0, 0, 1);
    clock_raw(0, 0, 1);
    clock_raw(0, 0, 1);
    clock_raw(0, 0, 1);
    clock_raw(0, 1, 1);
    clock_raw(0, 1, 1);
    clock_raw(0, 0, 1);
    clock_raw(0, 0, 1);
    clock_raw(0, 1, 1);
    clock_raw(0, 1, 1);
    clock_raw(0, 0, 1);
    clock_raw(0, 1, 1);
    clock_raw(0, 1, 1);
    clock_raw(0, 1, 1);
    clock_raw(0, 1, 1);
    clock_raw(0, 1, 1);
    clock_raw(0, 1, 1);
    clock_raw(0, 1, 1);
    clock_raw(0, 0, 1);
    clock_raw(0, 0, 1);
    clock_raw(0, 0, 1);
    clock_raw(0, 0, 1);
    clock_raw(0, 1, 96);

    // load the procedure into MPSSE
    for (int i = 0; i < cur_pos; i++) {
        unsigned int byte_sel = (i) / 8;
        unsigned int bit_sel = (i) % 8;
        int tdi = (arr_tdi[byte_sel] >> bit_sel) & 1;
        mpsse_clock_tms_cs(ctx, arr_tms, i, arr_tdo, i, 1, tdi, JTAG_MODE);
    }

    int rc = mpsse_flush(ctx);
    printf("mpsse_flush return code = %d\n", rc);

    printf("expected:\n");
    printf("  ff 77 c3 41 0a fc 03 b3 3f fc ff ff ff ff ff ff ff 03\n");

    printf("received:\n  ");
    for (int i = 0; i < (cur_pos+7)/8; i++) {
        printf("%02x ", arr_tdo[i]);
    }
    printf("\b\n");

    uint64_t jtag_ident = 0;
    memcpy(&jtag_ident, arr_tdo, sizeof(jtag_ident));
    jtag_ident >>= 6;
    jtag_ident >>= 4;
    jtag_ident &= 0xFFFFFFFFUL;
    printf("jtag_ident = %lx\n", jtag_ident);
}
