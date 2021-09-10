/*******************************************************************************
 * Freescale/NXP i.MX EPDC waveform assembler
 * 
 * This tools converts human readable .csv waveform file into .fw file used
 * by i.MX EPDC driver.
 * 
 * Copyright 2021 Wenting Zhang
 * 
 * This file is partially derived from Linux kernel driver, with the following
 * copyright information:
 * Copyright (C) 2014-2016 Freescale Semiconductor, Inc.
 * Copyright 2017 NXP
 ******************************************************************************/
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "ini.h"

#define MAX_MODES (32) // Maximum waveform modes supported
#define MAX_TEMPS (32) // Maximum temperature ranges supported

#define GREYSCALE_BPP   (2)
#define GREYSCALE_LEVEL (16)

typedef struct {
    unsigned int wi0;
    unsigned int wi1;
    unsigned int wi2;
    unsigned int wi3;
    unsigned int wi4;
    unsigned int wi5;
    unsigned int wi6;

    unsigned int xwia: 24; // address of extra waveform information
    unsigned int cs1: 8; // checksum 1

    unsigned int wmta: 24;
    unsigned int fvsn: 8;
    unsigned int luts: 8;
    unsigned int mc: 8; // mode count
    unsigned int trc: 8; // temperature range count
    unsigned int advanced_wfm_flags: 8;
    unsigned int eb: 8;
    unsigned int sb: 8;
    unsigned int reserved0_1: 8;
    unsigned int reserved0_2: 8;
    unsigned int reserved0_3: 8;
    unsigned int reserved0_4: 8;
    unsigned int reserved0_5: 8;
    unsigned int cs2: 8; // checksum 2
} waveform_data_header_t;

typedef struct {
    waveform_data_header_t wdh;
    uint8_t data[];    /* Temperature Range Table + Waveform Data */
} waveform_data_file_t;

typedef struct {
    uint8_t lut[GREYSCALE_LEVEL][GREYSCALE_LEVEL];
} waveform_lut_t;

typedef struct {
    char *prefix;
    int modes;
    char **mode_names;
    int *frame_counts;
    int temps;
    int *temp_ranges;
    waveform_lut_t **luts; // luts[mode][temp]
} context_t;

static int ini_parser_handler(void* user, const char* section, const char* name,
        const char* value) {
    context_t* pcontext = (context_t*)user;

    if (strcmp(section, "WAVEFORM") == 0) {
        if (strcmp(name, "VERSION") == 0) {
            assert(strcmp(value, "1.0") == 0);
        }
        else if (strcmp(name, "PREFIX") == 0) {
            pcontext->prefix = strdup(value);
        }
        else if (strcmp(name, "MODES") == 0) {
            // Allocate memory for modes
            pcontext->modes = atoi(value);
            assert(pcontext->modes <= MAX_MODES);
            pcontext->frame_counts = malloc(sizeof(int) * pcontext->modes);
            assert(pcontext->frame_counts);
            pcontext->mode_names = malloc(sizeof(char*) * pcontext->modes);
            assert(pcontext->mode_names);
        }
        else if (strcmp(name, "TEMPS") == 0) {
            // Allocate memory for temp ranges
            pcontext->temps = atoi(value);
            assert(pcontext->temps <= MAX_TEMPS);
            pcontext->temp_ranges = malloc(sizeof(int) * pcontext->temps);
            assert(pcontext->temp_ranges);
        }
        else {
            size_t len = strlen(name);
            if ((len >= 6) && (name[0] == 'M') &&
                    (strncmp(name + (len - 4), "NAME", 4) == 0)) {
                // Mode Name
                char *mode_id_s = strdup(name);
                mode_id_s[len - 4] = '\0';
                int mode_id = atoi(mode_id_s + 1);
                free(mode_id_s);
                pcontext->mode_names[mode_id] = strdup(value);
            }
            else if ((len >= 4) && (name[0] == 'M') &&
                    (strncmp(name + (len - 2), "FC", 2) == 0)) {
                // Frame Count
                char *mode_id_s = strdup(name);
                mode_id_s[len - 2] = '\0';
                int mode_id = atoi(mode_id_s + 1);
                free(mode_id_s);
                pcontext->frame_counts[mode_id] = atoi(value);
            }
            else if ((len >= 7) && (name[0] == 'T') &&
                    (strncmp(name + (len - 5), "RANGE", 5) == 0)) {
                // Temperature Range
                char *temp_id_s = strdup(name);
                temp_id_s[len - 5] = '\0';
                int temp_id = atoi(temp_id_s + 1);
                free(temp_id_s);
                pcontext->temp_ranges[temp_id] = atoi(value);
            }
            else {
                fprintf(stderr, "Unknown name %s=%s\n", name, value);
                return 0; // Unknown name
            }
        }
    }
    else {
        fprintf(stderr, "Unknown section %s\n", section);
        return 0; // Unknown section
    }
    return 1;
}

static void write_uint64_le(uint8_t* dst, uint64_t val) {
    dst[7] = (val >> 56) & 0xff;
    dst[6] = (val >> 48) & 0xff;
    dst[5] = (val >> 40) & 0xff;
    dst[4] = (val >> 32) & 0xff;
    dst[3] = (val >> 24) & 0xff;
    dst[2] = (val >> 16) & 0xff;
    dst[1] = (val >> 8) & 0xff;
    dst[0] = (val) & 0xff;
}

int main(int argc, char *argv[]) {
    context_t context;
    
    printf("Freescale/NXP i.MX EPDC waveform assembler\n");

    // Load waveform descriptor
    if (argc < 3) {
        fprintf(stderr, "Usage: ./wvfm_asm <input> <output>\n");
        fprintf(stderr, "The input file should be an ini waveform descriptor.\n"
                "The output file is .fw file for i.MX6/7 EPDC/EPDCv2.\n"
                "This tool is not compatible with i.MX5 EPDC.\n");
        return 1;
    }

    if (ini_parse(argv[1], ini_parser_handler, &context) < 0) {
        fprintf(stderr, "Failed to load waveform descriptor.\n");
        return 1;
    }

    // Print loaded info
    printf("Prefix: %s\n", context.prefix);

    for (int i = 0; i < context.modes; i++) {
        printf("Mode %d: %s, %d frames\n", i, context.mode_names[i],
                context.frame_counts[i]);
    }

    for (int i = 0; i < context.temps; i++) {
        printf("Temp %d: %d degC\n", i, context.temp_ranges[i]);
    }

    // Load actual waveform

    // Calculate file size and offset
    uint64_t header_size = sizeof(waveform_data_header_t);
    uint64_t temp_table_size = sizeof(uint8_t) * context.temps;
    uint64_t mode_offset_table_size = sizeof(uint64_t) * context.modes;
    uint64_t temp_offset_table_size = sizeof(uint64_t) * context.temps;

    // 1st level mode offset table
    uint64_t* mode_offset_table = malloc(mode_offset_table_size);
    // global offset table
    uint64_t* data_offset_table =
            malloc(sizeof(uint64_t) * context.modes * context.temps);
    uint64_t total_size = 0;
    uint64_t data_region_offset = temp_table_size + 1;
    total_size += mode_offset_table_size;

    for (int i = 0; i < context.modes; i++) {
        // Set the offset of the current mode
        mode_offset_table[i] = total_size;
        printf("Mode %d temp table offset %08llx\n", i, total_size);

        total_size += temp_offset_table_size;
        uint64_t mode_data_size = context.frame_counts[i] * 512;
        printf("Mode %d data size %llu bytes.\n", i, mode_data_size);
        for (int j = 0; j < context.temps; j++) {
            data_offset_table[i * context.temps + j] = total_size;
            printf("Mode %d Temp %d data offset %08llx\n", i, j, total_size);
            total_size += mode_data_size;
        }
    }
    total_size += header_size + temp_table_size + 1;

    // Allocate memory for waveform buffer
    waveform_data_file_t* pwvfm_file = malloc(total_size);
    assert(pwvfm_file);

    // Fill waveform header
    memset(&pwvfm_file->wdh, 0, sizeof(waveform_data_header_t));
    pwvfm_file->wdh.trc = context.temps - 1;
    pwvfm_file->wdh.mc = context.modes - 1;
    // Other fields (including checksums) are generally directly imported from
    // wbf file. They are not used in MXC EPDC driver. No need to fill them.

    // Fill temperature table
    for (int i = 0; i < context.temps; i++) {
        pwvfm_file->data[i] = context.temp_ranges[i];
    }

    // Fill waveform offset table and temp offset table
    uint8_t* wvfm_data_region = &pwvfm_file->data[data_region_offset];
    for (int i = 0; i < context.modes; i++) {
        write_uint64_le(&wvfm_data_region[i * 8],
                mode_offset_table[i]);
        for (int j = 0; j < context.temps; j++) {
            write_uint64_le(&wvfm_data_region[mode_offset_table[i] + j * 8],
                    data_offset_table[i * context.temps + j]);
        }
    }

    // Fill waveform data
    // TODO

    // Write waveform file
    FILE *outFile = fopen(argv[2], "wb");
    assert(outFile);

    size_t written = fwrite((uint8_t *)pwvfm_file, total_size, 1, outFile);
    assert(written == 1);

    fclose(outFile);

    return 0;
}