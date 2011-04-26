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

unsigned char *libmpq_sc2_readfile(MPQSC2 *scr, const char *filename)
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
    
    return read;
}

int main(int argc, char **argv)
{
    MPQSC2 *scr;
    
    if ((scr = libmpq_sc2_init("meta.SC2Replay")) == NULL) {
        printf("Init failed\n");
        return 0;
    }

    if (libmpq_sc2_readfile(scr, "replay.details") == NULL) {
        printf("Failed to read subfile\n");
        return;
    }
    

	return 1;
}
