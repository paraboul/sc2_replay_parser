/* (c) Anthony Catel */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpq.h>
#include "libmpq2.h"
#include <glob.h>

static int spaced = 0;
sc2_data_t *_libmpq_sc2_parse_serialzed_data(unsigned char *data, uint64_t size, 
    uint64_t *pos);
unsigned char stream_read(mpq_bitstream *stream);

uint8_t BITMASKS[9] = {0x00, 0x01, 0x03, 0x07, 0x0F, 0x1F, 0x3F, 0x7F, 0xFF};

#if 0
#define PRINTTAB(format, var)   for (x = 0; x < spaced; x++) { \
                            printf("\t"); \
                        } \
                        printf(format, var)
#endif
#if 1
#define PRINTTAB(format, var)
#endif

unsigned char stream_read_bits(mpq_bitstream *stream, uint8_t bits)
{
    uint8_t ret;
    if (stream->shift + bits >= 8) {
        stream->pos += (stream->shift + bits) / 8; /* TODO check this */
    }
    
    if ((stream->shift = (stream->shift + bits) % 8) == 0) {
        stream->pos -= 1; /* TODO check this */
        return stream_read(stream);
    }

    return (uint8_t)(stream->data[stream->pos] << (8 - stream->shift)) >> (8 - bits);

}

unsigned char stream_read(mpq_bitstream *stream)
{
    if (stream->pos+1 > stream->length) {
        printf("Buffer overflowed\n");
        return '\0';
    }
    stream->pos += 1;

    return (stream->shift ? 
                (uint8_t)(stream->data[stream->pos-1] & ~BITMASKS[stream->shift]) | 
                (uint8_t)(stream->data[stream->pos] & BITMASKS[stream->shift]) :
                
                stream->data[stream->pos - 1]);
}


uint32_t _libmpq_sc2_read_uint(mpq_bitstream *stream)
{
    return  stream_read(stream) << 24 |
            stream_read(stream) << 16 |
            stream_read(stream) << 8  |
            stream_read(stream);
}

uint16_t stream_read_short(mpq_bitstream *stream)
{
    if (!stream->shift) {
        return  stream_read(stream) << 8 |
                stream_read(stream);
    } else {
        uint8_t a, b;
        stream->pos += 2;
        
        a =  (uint8_t)(stream->data[stream->pos-2] & ~BITMASKS[stream->shift]) | 
             (uint8_t)(stream->data[stream->pos-1] >> (8-stream->shift));
        b =  ((uint8_t)(stream->data[stream->pos-1] << stream->shift) & ~BITMASKS[stream->shift]) | stream->data[stream->pos] & BITMASKS[stream->shift];
        
        return a << 8 | b;
    }
}

void stream_jump(mpq_bitstream *stream, uint32_t n)
{
    if (stream->pos+n > stream->length) {
        printf("Buffer overflowed\n");
        return;
    }
    
    stream->pos += n;
}

MPQSC2 *libmpq_sc2_init(const char *filename)
{
    mpq_archive_s *a;
    MPQSC2 *init;
    uint64_t pos = 0;
    uint32_t size;
    unsigned char *content;
    sc2_data_t *data;
    sc2_data_array_t *array;
    
    if (libmpq__archive_open(&a, filename, -1) != 0) {
        printf("No open\n");
        return NULL;
    }
    
    init = malloc(sizeof(*init));
    
    init->mpq = a;
    
    libmpq__archive_get_user_data(init->mpq, &content, &size);

    data = _libmpq_sc2_parse_serialzed_data(content, size, &pos);
    
    if (data == NULL) {
        free(init);
        /* todo close archive */
        printf("No data\n");
        return NULL;        
    }
    
    array = (sc2_data_array_t *)data->val.ptr;
    
    /* TODO : check overflow */
    init->duration = array->ptr[3]->val.integer;
    data = (sc2_data_t *)array->ptr[1];
    array = (sc2_data_array_t *)data->val.ptr;
    
    init->build = array->ptr[4]->val.integer;

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

    array->ptr       = malloc(sizeof(sc2_data_t *) * size);
    array->length    = size;
    array->pos       = 0;
    
    return array;
}

sc2_data_t *_libmpq_sc2_parse_serialzed_data(unsigned char *data, uint64_t size, 
    uint64_t *pos)
{
    #define CHECK_OVERFLOW(add) if (*pos+add > size) return NULL;
    sc2_data_t *new_val;
    int x;
    
    if (data == NULL) {
        return NULL;
    }
    new_val = malloc(sizeof(*new_val));


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
            
            PRINTTAB("Array(%lld) {\n", nelements);
            spaced++;
            
            while (nelements--) {
                sc2_data_t *ret;
                if ((ret = _libmpq_sc2_parse_serialzed_data(data, 
                                size, pos)) == NULL) {
                    return NULL;
                }
                new_array->ptr[new_array->pos++] = ret;                
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
            
            PRINTTAB("Keyval(%lld) {\n", nelements);
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
                
                new_array->ptr[new_array->pos++] = ret;
                
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
            
            PRINTTAB("Found vlf %lld\n", vlf);
            
            return new_val;
        }
        default:
            PRINTTAB("Unknow data %x\n", data[(*pos)-1]);
            
            break;
    }

    
    return NULL;
}


char *_libmpq_sc2_parse_replay_details(unsigned char *data, 
    uint64_t size)
{
    //#define CHECK_OVERFLOW() if (pos > size) return NULL
    uint64_t pos = 0;
    
    _libmpq_sc2_parse_serialzed_data(data, size, &pos);
    
    return NULL;

}

sc2_events_t *_libmpq_sc2_parse_message_events(unsigned char *data, uint64_t size)
{

    uint64_t pos = 0;
    int nevents;
    int events_size = 128;
    
    sc2_events_t *events = malloc(sizeof(*events) * events_size);
    
    while (pos < size) {
        int64_t ts;
        int flag;
        
        if (nevents > events_size) {
            events_size *= 2;
            events = realloc(events, sizeof(*events) * events_size);
        }
        pos += _libmpq_sc2_parse_timestamp(&data[pos], size, &ts);
        
        events[nevents].ts = ts;
        events[nevents].player_id = data[pos] & 0x0F;

        pos += 1;
        
        flag = data[pos++];
        
        if (flag == 0x83) {
            events[nevents].type = EVENT_1;
            pos += 8;
        } else if (flag == 0x80) {
            events[nevents].type = EVENT_2;
            pos += 4;
        } else if (!(flag & 0x80)) {
            int length;

            events[nevents].type = EVENT_MSG;
            
            length = (uint8_t)data[pos];
         
            if (flag & 0x08) {
                length += 64;
            } else if (flag & 0x10) {
                length += 128;
            }
            
            events[nevents].msg.val = (char *)&data[pos+1];
            events[nevents].msg.length = length;

            pos += length+1;

        } else {
            free(events);
            return NULL;
        }
        
        nevents++;
        
    }
    
    events[nevents].type = EVENT_END;
    return events;
}

void _libmpq_sc2_parse_events(mpq_bitstream *stream, int build)
{
    int iii = 0;
    unsigned char *data;
    int64_t pos;
    while (stream->pos < stream->length) {
        int64_t ts;
        uint8_t event_type, player_id, event_code;

        stream->pos += _libmpq_sc2_parse_timestamp(&stream->data[stream->pos], stream->length - stream->pos, &ts);
        iii++;

        event_type  = stream->data[stream->pos] >> 5;
        player_id   = stream->data[stream->pos] & 15;
        event_code  = stream->data[stream->pos+1];
        
        stream_jump(stream, 2);
        
        //printf("==== Event ====\nTimestamp : %lld\nEvent Type : %.2x\nPlayer id : %d\nEvent code : %.2x\n", ts, event_type, player_id, event_code);
        //printf("(%d) Type %d - Code %x - Start 0 - TS : %lld\n", iii, event_type, event_code, ts);
        switch(event_type) {
            case 0x00:
                switch(event_code) {
                    case 0x2C:
                    case 0x0C:
                    case 0x0B:
                        break;
                    case 0x05:
                        break;
                    default:
                        break;
                }

                break;
            case 0x01:
                switch(event_code) {
                    case 0x09:
                        printf("Player leave the game\n");
                        break;
                    case 0x0D:
                    case 0x1D:
                    case 0x2D:
                    case 0x3D:
                    case 0x4D:
                    case 0x5D:
                    case 0x6D:
                    case 0x7D:
                    case 0x8D:
                    case 0x9D:
                    {
                        uint8_t action, mode;

                        action = stream_read_bits(stream, 2);
                        mode = stream_read_bits(stream, 2);

                        if (mode == 1) {
                            uint8_t length;
                            
                            length = stream_read(stream);

                            stream_read_bits(stream, length);

                        } else if (mode == 2 || mode == 3) {
                            uint8_t index_length;
                            int j;
                            
                            index_length = stream_read(stream);
                            
                            for (j = 0; j < index_length; j++) {
                                stream_read(stream);
                            }
                            //return;
                            break;     
                        }

                        break;                    
                    }
                    case 0x0B:
                    case 0x1B:
                    case 0x2B:
                    case 0x3B:
                    case 0x4B:
                    case 0x5B:
                    case 0x6B:
                    case 0x7B:
                    case 0x8B:
                    case 0x9B:
                    {
                        uint8_t flags = stream_read(stream);
                        uint8_t atype = stream_read(stream);
                        uint32_t ability;
                        
                        if (build >= 18574) {
                            if (atype & 0x20) {
                                printf("Unhandled 0x20\n");
                                return;
                            } else if (atype & 0x40) {
                                uint8_t hinge;

                                stream_jump(stream, 2);
                                
                                hinge = stream_read(stream);
                                
                                if (hinge & 0x20) {
                                    stream_jump(stream, 9);
                                } else if (hinge & 0x40) {
                                    stream_jump(stream, 18);
                                } else {
                                    break;
                                }
                        
                            } else if (atype & 0x80) {
                                uint32_t object_id, object_type;
                                ability = (stream_read(stream) << 8) | stream_read(stream);
                                
                                object_id = _libmpq_sc2_read_uint(stream);
                                object_type = stream_read_short(stream);
                                
                                stream_jump(stream, 10);                  
                            } else if (atype < 0x10) {
                                stream_jump(stream, 10);
                        
                            }
                        } else {
                        
                            if (atype & 0x20) {
                                uint32_t object_id;
                                ability = stream_read(stream) << 8 | stream_read(stream);

                                if (flags == 0x29 || flags == 0x19 || flags == 0x14) {
                                
                                } else {
                                    uint8_t ability_flags;
                                    
                                    ability_flags = stream_read_bits(stream, 6);

                                    ability = ability << 8 | ability_flags;
                                    
                                    if (ability_flags & 0x10) {
                                        stream_jump(stream, 9);
                                    } else if (ability_flags & 0x20) {
                                        uint16_t code, object_type;
                                        uint32_t object_id;
                                        
                                        code = stream_read_short(stream);
                                        
                                        /* TODO : uint bitshiftet are not handled */
                                        object_id = _libmpq_sc2_read_uint(stream);
                                        object_type = stream_read_short(stream);
                                        
                                        stream_jump(stream, 10);


                                    } else {
                                        //printf("Unknow command card ability\n");
                                    }
                                }

                            } else if (atype & 0x40) {
                                if (flags == 0x08) {
                                    stream_jump(stream, 10);
                                } else {
                                    printf("Unhandled move/location %x\n", atype);
                                    return;
                                }
                            } else if (atype & 0x80) {
                                uint32_t object_id, object_type;
                                ability = (stream_read(stream) << 8) | stream_read(stream);
                                
                                object_id = _libmpq_sc2_read_uint(stream);
                                object_type = stream_read_short(stream);
                                
                                #if 0
                                printf("Ability : %d\n", ability);
                                printf("Object_id : %d\n", object_id);
                                printf("Object type : %d\n", object_type);
                                #endif
                                stream_jump(stream, 10);
                            } else {
                                printf("Unknown event\n");
                                return;
                            }
                        }
                        break;
                    }
                    case 0x0C:
                    case 0x1C:
                    case 0x2C:
                    case 0x3C:
                    case 0x4C:
                    case 0x5C:
                    case 0x6C:
                    case 0x7C:
                    case 0x8C:
                    case 0x9C:
                    case 0xAC:
                    {
                        uint8_t flag, deselect_flag, unit_type_n, unit_n;
                        int j;
                        
                        flag = stream_read(stream);
                        deselect_flag = stream_read_bits(stream, 2);
                        
                        switch(deselect_flag) {
                            case 0x01:
                            {
                                uint8_t bits;
                                
                                bits = stream_read(stream);

                                stream_read_bits(stream, bits);


                                break;
                            }
                            case 0x02:
                            case 0x03:
                            {
                                uint8_t index_length;
                                int j;
                                
                                index_length = stream_read(stream);
                                
                                for (j = 0; j < index_length; j++) {
                                    stream_read(stream);
                                }
                                
                                break;
                            }
                            default:
                                break;
                        }
                        
                        unit_type_n = stream_read(stream);
                        //printf("Unit type n : %d\n", unit_type_n);
                        for (j = 0; j < unit_type_n; j++) {
                            #if 1
                            stream_read_short(stream);
                            stream_read(stream);
                            stream_read(stream);
                            #endif
                            #if 0
                            printf("uID : %d\n", (stream_read_short(stream) << 8) | stream_read(stream));
                            printf("Count : %d\n", stream_read(stream));
                            #endif
                        }
                        
                        unit_n = stream_read(stream);
                        
                        for (j = 0; j < unit_n; j++) {
                            #if 1
                            stream_read_short(stream);
                            stream_read_short(stream);
                            #endif
                            #if 0
                            printf("Recyled : %d\n", stream_read_short(stream));
                            printf("Counter : %d\n", stream_read_short(stream));
                            #endif
                        }

                        break;
                    }
                    default:
                        printf("UNHANDLED\n");
                        return;
                        break;
                }
                
                break;
            case 0x02:
                printf("Event 0x02\n");
                return;
                break;
            case 0x03:
                if ((event_code & 0x0F) == 0x01) {
                    uint8_t cur;

                    stream_jump(stream, 3);
                    
                    cur = stream_read(stream);
                    //printf("Cur : %.2x\n", cur & 0x70);
                    switch((cur & 0x70)) {
                        case 0x10:
                        case 0x30:
                        case 0x50:
                            stream_jump(stream, 1);
                            cur = stream_read(stream);
                        case 0x20:
                            if ((cur & 0x20) > 0) {
                                stream_jump(stream, 1);
                                cur = stream_read(stream);
                                //printf("wut\n");
                            }
                            if ((cur & 0x40) == 0) break;
                            //printf("then\n");
                        case 0x40:
                            stream_jump(stream, 2);
                            break;
                        default:
                            break;
                    }

                    break;
                }
                switch(event_code) {
                    case 0x08:
                        //printf("there %.2x\n\n\n\n", data[++pos]);
                        stream_jump(stream, 1);
                        break;
                    default:
                        printf("Unknow camera %.2x\n", event_code);
                        break;
                }
                return;
                break;
            case 0x04:
                switch(event_code) {
                    case 0x00:
                        printf("JUMP %d\n", iii);
                        return;
                        //stream_jump(stream, 10);
                        break;
                    default:
                        printf("Event 0x04 %x\n", event_code);
                        return;
                        break;
                }

                break;
            case 0x05:
                printf("Event 0x05\n");
                return;
                break;
            default:
                printf("Failed\n");
                return;
                break;
        }
        if (stream->shift != 0) {
            stream->shift = 0;
            stream->pos += 1;
        }
        

    }
    
    printf("Finished with %d events\n", iii);
    
    return;

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
    mpq_bitstream stream;
    unsigned char *content;
    int64_t pos = 0;
    int64_t size = 0;
    int i;
    //sc2_events_t *msg;
    
	glob_t globbuf;
	glob("./replays/*.sc2replay", 0, NULL, &globbuf);

	for (i = 0; i < globbuf.gl_pathc; i++) {
	    printf("File : %s\n", globbuf.gl_pathv[i]);

        if ((scr = libmpq_sc2_init(globbuf.gl_pathv[i])) == NULL) {
            printf("Init failed\n");
            continue;
        }
        
        #if 0
        if ((content = libmpq_sc2_readfile(scr, "replay.details", &size)) == NULL) {
            printf("Failed to read subfile\n");
            return 0;
        }

        _libmpq_sc2_parse_replay_details(content, size);
        #endif
        /*
        if ((content = libmpq_sc2_readfile(scr, "replay.message.events", &size)) == NULL) {
            printf("Failed to read subfile\n");
            return 0;
        }

        msg = _libmpq_sc2_parse_message_events(content, size);*/


        if ((content = libmpq_sc2_readfile(scr, "replay.game.events", &size)) == NULL) {
            printf("Failed to read subfile\n");
            return 0;
        }


        stream.data = content;
        stream.pos = 0;
        stream.length = size;
        stream.shift = 0;
        
        printf("Start...\n");
        _libmpq_sc2_parse_events(&stream, scr->build);


    }
    
	return 1;
}

