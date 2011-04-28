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

int64_t _libmpq_sc2_parse_timestamp(unsigned char *data, uint64_t size,
    int64_t *res)
{
    uint8_t n = 0, extra = 0;
    *res = 0;   
    
    extra = data[0] & 0x03;
    
    *res = data[0] >> 2;
    
    for (n = 0; n < extra; n++) {
        *res <<= 8;
        *res |= data[n+1];  
    }
    
    return extra+1;
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
                    return NULL;
                }

                if ((ret = _libmpq_sc2_parse_serialzed_data(data, 
                                size, pos)) == NULL) {
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


SC2_REPLAY_DETAILS *_libmpq_sc2_parse_replay_details(unsigned char *data, 
    uint64_t size)
{
    //#define CHECK_OVERFLOW() if (pos > size) return NULL
    uint64_t pos = 0;
    
    _libmpq_sc2_parse_serialzed_data(data, size, &pos);
    
    return NULL;

}


void _libmpq_sc2_parse_message_events(unsigned char *data, uint64_t size)
{
    uint64_t pos = 0; 

    while (pos < size) {
        int64_t ts;
        int flag;
        
        pos += _libmpq_sc2_parse_timestamp(&data[pos], size, &ts);
        pos += 1;
        
        flag = data[pos++];
        
        if (flag == 0x83) {
            printf("Event 1\n");
            pos += 8;
        } else if (flag == 0x80) {
            printf("Event 2\n");
            pos += 4;
        } else if (!(flag & 0x80)) {
            int length;
            char msg[512];
            memset(msg, '\0', 512);

            length = (uint8_t)data[pos];
         
            if (flag & 0x08) {
                length += 64;
            } else if (flag & 0x10) {
                length += 128;
            }

            memcpy(msg, &data[pos+1], length);
            pos += length+1;
            
            printf("Message : %s\n", msg);
            
        } else {
            printf("Le fu\n");
        }
        
    }
    
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

    
    if ((scr = libmpq_sc2_init("talk.sc2replay")) == NULL) {
        printf("Init failed\n");
        return 0;
    }

   /* if ((content = libmpq_sc2_readfile(scr, "replay.details", &size)) == NULL) {
        printf("Failed to read subfile\n");
        return 0;
    }

    _libmpq_sc2_parse_replay_details(content, size);*/
    
    if ((content = libmpq_sc2_readfile(scr, "replay.message.events", &size)) == NULL) {
        printf("Failed to read subfile\n");
        return 0;
    }
    printf("Start : %d\n", size);
    _libmpq_sc2_parse_message_events(content, size);

	return 1;
}

