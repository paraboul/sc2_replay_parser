#ifndef __MPQ2_H__
#define __MPQ2_H__ 1

#include <sys/types.h>
#include <stdint.h>
#include <libmpq/mpq.h>

#define MPQ_MAGIC 0x1B51504D

typedef struct _mpqsc2
{
    mpq_archive_s *mpq;
    
} MPQSC2;

#endif

