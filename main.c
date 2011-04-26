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
    
    if (libmpq__archive_open(&a, "meta.SC2Replay", -1) != 0) {
        return NULL;
    }
    
    init = malloc(sizeof(*init));
    
    init->mpq = a;
    
    return init;
    
}

uint64_t _libmpq_sc2_parse_player_details(unsigned char *data, uint64_t size,
    SC2_PLAYERS_DETAILS **player)
{
    uint64_t pos;
    SC2_PLAYERS_DETAILS_STATE state = PS_READ_START;
    
    *player = malloc(sizeof(SC2_PLAYERS_DETAILS));
    (*player)->short_name.val = NULL;
    (*player)->full_name.val = NULL;
    (*player)->race.val = NULL;
    
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
                (*player)->short_name.val = malloc((*player)->short_name.length);
                state = PS_READ_SHORTNAME;
                break;
            case PS_READ_SHORTNAME:
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

