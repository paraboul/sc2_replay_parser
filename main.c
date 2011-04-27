/* (c) Anthony Catel */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libmpq/mpq.h>
#include "libmpq2.h"

MPQSC2 *libmpq_sc2_init(const char *filename)
{
    mpq_archive_s *a;
    MPQSC2 *init;
    
    if (libmpq__archive_open(&a, filename, -1) != 0) {
        return NULL;
    }
    
    init = malloc(sizeof(*init));
    
    init->mpq = a;
    
    return init;
    
}

int64_t _libmpq_sc2_parse_vlf(unsigned char *data, uint64_t size)
{
    uint64_t pos;
    int64_t result = 0;
    
    for (pos = 0; pos < size; pos++) {
        result |= (data[pos] & 0x7F) << (pos * 7);
        if (!(data[pos] & 0x80)) {
            return (result >> 1) * (result & 0x01 ? -1 : 1);
        }
    }
    
    return 0;
}

uint64_t _libmpq_sc2_parse_player_details(unsigned char *data, uint64_t size,
    SC2_PLAYERS_DETAILS **player)
{
    int reg = 0;
    uint64_t pos;
    SC2_PLAYERS_DETAILS_STATE state = PS_READ_START;
    
    *player = malloc(sizeof(SC2_PLAYERS_DETAILS));
    
    INIT_STRING((*player)->short_name);
    INIT_STRING((*player)->full_name);
    INIT_STRING((*player)->race);

    for (pos = 0; pos < size; pos++) {
        switch(state) {
            case PS_READ_START:
                if (pos == 3) {
                    state = PS_READ_SHORTNAME_LENGTH;
                }
                break;
            case PS_READ_SHORTNAME_LENGTH:
                (*player)->short_name.length = (uint8_t)data[pos] / 2;
                
                if ((*player)->short_name.length > 64) {
                    goto error;
                }
                
                (*player)->short_name.val = 
                                    malloc((*player)->short_name.length + 1);
                reg = 0;
                state = PS_READ_SHORTNAME;
                
                break;
            case PS_READ_SHORTNAME:
                (*player)->short_name.val[reg++] = (char)data[pos];
                
                if (reg == (*player)->short_name.length) {
                    (*player)->short_name.val[reg] = '\0';
                    printf("Player 1 : %s\n", (*player)->short_name.val);
                    return 0;
                }
                break;
            default:
                break;
        }
    }
    
    return pos;
    
error:
    free((*player)->short_name.val);
    free((*player)->full_name.val);
    free((*player)->race.val);
    
    free(*player);
    
    return 0;
}

SC2_REPLAY_DETAILS *_libmpq_sc2_parse_replay_details(unsigned char *data, 
    uint64_t size)
{
    #define CHECK_OVERFLOW() if (pos > size) return NULL
    uint64_t pos;
    
    SC2_REPLAY_DETAILS *details = malloc(sizeof(*details));
    
    details->nplayers           = 0;
    details->map_name           = NULL;
    details->minimap_filename   = NULL;
    details->map_path           = NULL;
    
    for (pos = 0; pos < size; pos++) {
        switch(pos) {
            case 6:
                details->nplayers = (uint8_t)data[pos] / 2;
                break;
            case 7:
            {
                uint8_t pstruct;

                for (pstruct = 0; pstruct < 16; pstruct++) {
                    SC2_PLAYERS_DETAILS *player;
                    
                    pos += _libmpq_sc2_parse_player_details(&data[pos], 
                                size-pos, &player);
                                
                    CHECK_OVERFLOW();
                    return NULL;
                }
            }
            break;
        }
    }
    
    return details;
}

unsigned char *libmpq_sc2_readfile(MPQSC2 *scr, const char *filename, libmpq__off_t *size)
{
    uint32_t filenumber = 0;
    libmpq__off_t filesize = 0, transferred = 0;
    unsigned char *read;
    
    if (libmpq__file_number(scr->mpq, filename, &filenumber) == 
            LIBMPQ_ERROR_EXIST) {
        
        return NULL;
    }
    
    libmpq__file_unpacked_size(scr->mpq, filenumber, &filesize);

    read = malloc(filesize);
    
    if (libmpq__file_read(scr->mpq, filenumber, 
                            read, filesize, &transferred) != 0) {
        free(read);
        return NULL;
    }
    
    *size = transferred;
    
    return read;
}

int main(int argc, char **argv)
{
    MPQSC2 *scr;
    unsigned char *content;
    int64_t size = 0;
    
    unsigned char lefu[4];
    
    lefu[0] = 0x82;
    lefu[1] = 0x01;
    lefu[2] = 0x80;
    lefu[3] = 0x10;
    
    printf("Res : %d\n", _libmpq_sc2_parse_vlf(lefu, 2));
    
    if ((scr = libmpq_sc2_init("1v1.sc2replay")) == NULL) {
        printf("Init failed\n");
        return 0;
    }

    if ((content = libmpq_sc2_readfile(scr, "replay.details", &size)) == NULL) {
        printf("Failed to read subfile\n");
        return 0;
    }
    
    _libmpq_sc2_parse_replay_details(content, size);

	return 1;
}

