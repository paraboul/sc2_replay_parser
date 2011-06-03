#ifndef __MPQ2_H__
#define __MPQ2_H__ 1

#include <sys/types.h>
#include <stdint.h>
#include <libmpq/mpq.h>

#define MPQ_MAGIC 0x1B51504D


#define INIT_STRING(str) do { str.val = NULL; str.length = 0; } while(0)

typedef struct _sc2string {
    char *val;
    unsigned int length;
} sc2string;


typedef struct _mpqstream {
    unsigned char *data;
    int32_t length;
    
    int32_t pos;
    
    uint8_t shift;
} mpq_bitstream;

typedef struct _mpqsc2
{
    mpq_archive_s *mpq;
    char *version;
    uint32_t build;
    uint64_t duration;
    
} MPQSC2;

typedef enum {
    EVENT_1,
    EVENT_2,
    EVENT_MSG,
    EVENT_END
} sc2_event_e;

typedef enum {
    SC2_DATA_STRING,
    SC2_DATA_ARRAY,
    SC2_DATA_KEYVAL,
    SC2_DATA_INT
} sc2_data_e;

typedef struct _sc2_events
{
    sc2_data_e type;
    uint8_t player_id;
    
    sc2string msg;
    int64_t ts;
} sc2_events_t;

typedef struct _sc2_data
{
    sc2_data_e type;
    
    union {
        sc2string str;
        void *ptr;
        uint64_t integer;
    } val;
    
} sc2_data_t;

typedef struct _sc2_data_array
{
    uint32_t length;
    uint32_t pos;
    sc2_data_t *ptr;
    
} sc2_data_array_t;

#ifndef MIN
    #define MIN(val1, val2) ((val1 > val2) ? (val2) : (val1))
    #define MAX(val1, val2) ((val1 < val2) ? (val2) : (val1))
#endif

#endif

