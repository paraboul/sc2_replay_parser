/* (c) Anthony Catel */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libmpq/mpq.h>
#include "libmpq2.h"

static int spaced = 0;

#define PRINTTAB(format, var)   for (x = 0; x < spaced; x++) { \
                            printf("\t"); \
                        } \
                        printf(format, var)

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

int64_t _libmpq_sc2_parse_vlf(unsigned char *data, uint64_t size, int64_t *res)
{
    uint64_t pos;
    int64_t result = 0;
    *res = -1;
    
    for (pos = 0; pos < size; pos++) {
        result |= (data[pos] & 0x7F) << (pos * 7);
        if (!(data[pos] & 0x80)) {
            *res = (result >> 1) * (result & 0x01 ? -1 : 1);
            return pos+1;
        }
    }
    return pos;
}

sc2_data_array_t *_libmpq_sc2_init_data_array(uint32_t size)
{
    sc2_data_array_t *array = malloc(sizeof(*array));
    sc2_data_t *sc2_data = malloc(sizeof(sc2_data_t) * size);
    
    array->ptr       = sc2_data;
    array->length    = size;
    array->pos       = 0;
    
    return array;
}

sc2_data_t *_libmpq_sc2_parse_serialzed_data(unsigned char *data, uint64_t size, 
    uint64_t *pos)
{
    #define CHECK_OVERFLOW(add) if (*pos+add > size) return NULL;

    sc2_data_t *new_val = malloc(sizeof(*new_val));
    
    int x;

    switch(data[(*pos)++]) {
        case 0x02: /* Byte string */
        {
            int64_t length;
            char tmp[256];
            
            memset(tmp, '\0', 256);
            
            new_val->type = SC2_DATA_STRING;
            
            if ((*pos += _libmpq_sc2_parse_vlf(&data[*pos], size-*pos, 
                    &length)) == -1) {
                return NULL;
            }
            CHECK_OVERFLOW(length);
            
            new_val->val.str.val    = (char *)&data[*pos];
            new_val->val.str.length = length;
            
            *pos += length;
            
            memcpy(tmp, new_val->val.str.val, length);
            
            PRINTTAB("String : %s\n", tmp);
            
            return new_val;
        }
        case 0x04: /* Array */
        {
            int64_t nelements;
            sc2_data_array_t *new_array;
            CHECK_OVERFLOW(2);
            *pos += 2;
            
            new_val->type = SC2_DATA_ARRAY;

            if ((*pos += _libmpq_sc2_parse_vlf(&data[*pos], size-*pos, 
                        &nelements)) == -1 || nelements > 2048) {

                return NULL;
            }
            new_array = _libmpq_sc2_init_data_array(nelements);
            
            new_val->val.ptr = new_array;
            
            PRINTTAB("Array(%d) {\n", nelements);
            spaced++;
            
            while (nelements--) {
                sc2_data_t *ret;
                if ((ret = _libmpq_sc2_parse_serialzed_data(data, 
                                size, pos)) == NULL) {
                    printf("FAILED 3\n");
                    return NULL;
                }
                new_array[new_array->pos++].ptr = ret;                
            }
            spaced--;
            
            PRINTTAB("}%c", '\n');

            return new_val;
        }
        case 0x05: /* Key-value */
        {
            int64_t nelements;
            sc2_data_array_t *new_array;
            
            new_val->type = SC2_DATA_KEYVAL;

            
            if ((*pos += _libmpq_sc2_parse_vlf(&data[*pos], size-*pos, 
                        &nelements)) == -1 || nelements > 2048) {
                return NULL;
            }
            
            new_array = _libmpq_sc2_init_data_array(nelements);

            new_val->val.ptr = new_array;
            
            PRINTTAB("Keyval(%d) {\n", nelements);
            spaced++;
            while (nelements--) {
                int64_t key;
                sc2_data_t *ret;
                            
                if ((*pos += _libmpq_sc2_parse_vlf(&data[*pos], size-*pos, 
                            &key)) == -1) {
                    printf("FAILED\n");
                    return NULL;
                }

                if ((ret = _libmpq_sc2_parse_serialzed_data(data, 
                                size, pos)) == NULL) {
                    printf("FAILED 2\n");
                    return NULL;
                }
                
                new_array[new_array->pos++].ptr = ret;
                
            }
            spaced--;
            PRINTTAB("}%c", '\n');
        
            return new_val;     
        }
        case 0x06: /* Single byte int */
            
            new_val->type = SC2_DATA_INT;
            new_val->val.integer = (uint8_t)data[*pos];
            PRINTTAB("(int) %d\n", (uint8_t)new_val->val.integer);
            *pos = *pos+1;
            return new_val;
        case 0x07: /* 4 byte int */
            
            CHECK_OVERFLOW(3);
            new_val->type = SC2_DATA_INT;
            new_val->val.integer =  data[*pos] | 
                                    data[*pos+1] << 8 | 
                                    data[*pos+2] << 16 | 
                                    data[*pos+3] << 24;
            
            *pos += 4;
            PRINTTAB("(bigint) %lld\n", new_val->val.integer);
            return new_val;
        case 0x09: /* VLF */
        {
            int64_t vlf;
            if ((*pos += _libmpq_sc2_parse_vlf(&data[*pos], size-*pos, 
                        &vlf)) == -1) {
                return NULL;
            }            
            new_val->type = SC2_DATA_INT;
            new_val->val.integer = vlf;
            
            PRINTTAB("Found vlf %d\n", vlf);
            
            return new_val;
        }
        default:
            PRINTTAB("Unknow data %x\n", data[(*pos)-1]);
            break;
    }

    
    return NULL;
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
    //#define CHECK_OVERFLOW() if (pos > size) return NULL
    uint64_t pos = 0;
    
    _libmpq_sc2_parse_serialzed_data(data, size, &pos);
    
    return NULL;
    
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
                                
                    //CHECK_OVERFLOW();
                    return NULL;
                }
            }
            break;
        }
    }
    
    return details;
}

unsigned char *libmpq_sc2_readfile(MPQSC2 *scr, const char *filename,
    libmpq__off_t *size)
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

    
    if ((scr = libmpq_sc2_init("meta.SC2Replay")) == NULL) {
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

