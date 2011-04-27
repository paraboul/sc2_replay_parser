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


typedef struct _mpqsc2
{
    mpq_archive_s *mpq;
    
} MPQSC2;

typedef enum {
    PS_READ_START,
    PS_READ_SHORTNAME_LENGTH,
    PS_READ_SHORTNAME
    
} SC2_PLAYERS_DETAILS_STATE;

typedef struct _sc2_player_defails
{
    sc2string short_name;
    sc2string full_name;
    sc2string race;
    
    struct _sc2_player_defails *next;
    
} SC2_PLAYERS_DETAILS;


typedef struct _sc2_replay_details
{
    uint8_t nplayers;
    
    char *map_name;
    char *minimap_filename;
    char *map_path;
    
    SC2_PLAYERS_DETAILS *players;
} SC2_REPLAY_DETAILS;

typedef enum {
    SC2_DATA_STRING,
    SC2_DATA_ARRAY,
    SC2_DATA_KEYVAL,
    SC2_DATA_INT
} sc2_data_e;

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

