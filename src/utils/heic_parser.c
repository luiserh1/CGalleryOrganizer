#include <stdio.h>
#include <string.h>

#include "heic_parser.h"

#define HEIC_SIGNATURE_SIZE 12
static const unsigned char HEIC_SIGNATURE[HEIC_SIGNATURE_SIZE] = {
    0x00, 0x00, 0x00, 0x18, 'f', 't', 'y', 'p',
    'h', 'e', 'i', 'c'};

static const unsigned char HEIC_ALT_SIGNATURE[8] = {'h', 'e', 'i', 'c', 0};

#define BOX_ftyp 0x66747970
#define BOX_moov 0x6D6F6F76
#define BOX_mvhd 0x6D766864
#define BOX_trak 0x7472616B
#define BOX_tkhd 0x746B6864
#define BOX_avc1 0x61766331
#define BOX_avc3 0x61766333
#define BOX_hvc1 0x68766331
#define BOX_hvc3 0x68766333
#define BOX_encv 0x656E6376
#define BOX_ispe 0x69737065

typedef struct {
    unsigned int box_size;
    unsigned int box_type;
} BoxHeader;

static unsigned int read_u32_be(const unsigned char *buf) {
    return ((unsigned int)buf[0] << 24) | ((unsigned int)buf[1] << 16) |
           ((unsigned int)buf[2] << 8) | (unsigned int)buf[3];
}

static int read_box_header(FILE *f, BoxHeader *header) {
    unsigned char buf[8];
    if (fread(buf, 1, 8, f) != 8)
        return -1;

    header->box_size = read_u32_be(buf);
    header->box_type = read_u32_be(buf + 4);

    return 0;
}

static int find_ispe_box(FILE *f, unsigned int box_size, int *width,
                         int *height) {
    while (box_size > 8) {
        BoxHeader child;
        if (read_box_header(f, &child) != 0)
            return -1;

        if (child.box_type == BOX_ispe) {
            unsigned char data[8];
            if (fread(data, 1, 8, f) != 8)
                return -1;

            *width = (int)read_u32_be(data);
            *height = (int)read_u32_be(data + 4);
            return 0;
        }

        unsigned int child_size = child.box_size;
        if (child_size == 0)
            child_size = box_size - 8;
        else if (child_size < 8)
            child_size = 8;

        fseek(f, child.box_size - 8, SEEK_CUR);
        box_size -= child_size;
    }

    return -1;
}

static int find_dimension_in_moov(FILE *f, int *width, int *height) {
    BoxHeader moov;
    if (read_box_header(f, &moov) != 0)
        return -1;

    if (moov.box_type != BOX_moov)
        return -1;

    unsigned int moov_size = moov.box_size;
    while (moov_size > 8) {
        BoxHeader trak;
        if (read_box_header(f, &trak) != 0)
            return -1;

        if (trak.box_type == BOX_trak) {
            unsigned int track_end = ftell(f) + trak.box_size - 8;

            while (ftell(f) < track_end && trak.box_size > 8) {
                BoxHeader tkhd;
                if (read_box_header(f, &tkhd) != 0)
                    return -1;

                if (tkhd.box_type == BOX_tkhd) {
                    unsigned char tkhd_data[4];
                    fseek(f, 12, SEEK_CUR);
                    if (fread(tkhd_data, 1, 4, f) == 4) {
                        *height =
                            (tkhd_data[0] << 24) | (tkhd_data[1] << 16) |
                            (tkhd_data[2] << 8) | tkhd_data[3];
                    }
                    fseek(f, 4, SEEK_CUR);
                    if (fread(tkhd_data, 1, 4, f) == 4) {
                        *width = (tkhd_data[0] << 24) | (tkhd_data[1] << 16) |
                                 (tkhd_data[2] << 8) | tkhd_data[3];
                        return 0;
                    }
                }

                fseek(f, tkhd.box_size - 8, SEEK_CUR);
                trak.box_size -= tkhd.box_size;
            }
        }

        fseek(f, trak.box_size - 8, SEEK_CUR);
        moov_size -= trak.box_size;
    }

    return -1;
}

HeicData HeicParse(const char *filepath) {
    HeicData metadata = {0};

    if (!filepath)
        return metadata;

    FILE *f = fopen(filepath, "rb");
    if (!f)
        return metadata;

    unsigned char signature[12];
    if (fread(signature, 1, 12, f) != 12) {
        fclose(f);
        return metadata;
    }

    int is_heic = 0;
    if (memcmp(signature + 4, "ftyp", 4) == 0) {
        if (memcmp(signature + 8, "heic", 4) == 0 ||
            memcmp(signature + 8, "mif1", 4) == 0 ||
            memcmp(signature + 8, "msf1", 4) == 0) {
            is_heic = 1;
        }
    }

    if (!is_heic) {
        fseek(f, 0, SEEK_SET);
        if (fread(signature, 1, 8, f) == 8) {
            if (memcmp(signature, "heic", 4) == 0)
                is_heic = 1;
        }
    }

    if (!is_heic) {
        fclose(f);
        return metadata;
    }

    fseek(f, 0, SEEK_SET);
    BoxHeader box;
    int width = 0, height = 0;

    while (read_box_header(f, &box) == 0) {
        if (box.box_type == BOX_moov) {
            if (find_dimension_in_moov(f, &width, &height) == 0) {
                metadata.width = width;
                metadata.height = height;
                break;
            }
        }

        if (box.box_size == 0)
            break;

        fseek(f, box.box_size - 8, SEEK_CUR);
    }

    fclose(f);

    return metadata;
}

bool HeicGetDimensions(const char *filepath, int *width, int *height) {
    if (!filepath || !width || !height)
        return false;

    HeicData metadata = HeicParse(filepath);
    if (metadata.width <= 0 || metadata.height <= 0)
        return false;

    *width = metadata.width;
    *height = metadata.height;

    return true;
}
