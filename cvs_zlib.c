/*****************************************************************************
 * Filename: cvs_zlib.c
 * This file contains functions to deal with gzip'ed data read from CVS.
 ****************************************************************************/


#include <zlib.h>

#include "cvs_cmds.h"


/* Uncompress a cvs data buffer and return the true size of the data */
int cvs_zlib_inflate_buffer(cvs_buff *input_buff, int in_size, int in_offset, 
                            char *output, int out_size, int keep_data)
{
    z_stream stream;
    int status;
    int size;
    unsigned char *buf = (input_buff->data + input_buff->cookie);
    int pos;
    char *keep_buff;
    int keep_chunk;
    int num_chunks = 0;

    if (keep_data)
    {
        keep_buff = malloc(out_size);
    }
    
    keep_chunk = in_offset / out_size;

    stream.next_in = (Bytef *)buf;
    stream.avail_in = in_size;
    
    stream.next_out = (Bytef *)output;
    stream.avail_out = out_size;
    
    stream.zalloc = Z_NULL;
    stream.zfree = Z_NULL;
    stream.opaque = 0;
    
    /* Undocumented feature: negative argument makes zlib skip the gzip header */
    status = inflateInit2(&stream, -15);
    if (status != Z_OK)
    {
        printf("error is %d\n", status);
        return 0;
    }

    /* Check the gzip header */
    if (buf[0] != 31 || buf[1] != 139)
    {
        fprintf(stderr, "gzipped data does not start with gzip identification\n");
        return 0;
    }
    if (buf[2] != 8)
    {
        fprintf(stderr, "only the deflate compression method is supported\n");
        return 0;
    }
    
    pos = 10;
    if (buf[3] & 4)
    {
        pos += buf[pos] + (buf[pos + 1] << 8) + 2;
    }
    if (buf[3] & 8)
    {
        pos += strlen (buf + pos) + 1;
    }
    if (buf[3] & 16)
    {
        pos += strlen (buf + pos) + 1;
    }
    if (buf[3] & 2)
    {
        pos += 2;
    }
    
    stream.next_in = (Bytef *)buf + pos;
    stream.avail_in = in_size - pos;
    
    /* Uncompress all of the compressed data */
    while (status != Z_STREAM_END)
    {
        status = inflate(&stream, Z_NO_FLUSH);
        
        if (status == Z_OK)
        {
            /* The output buffer is full */
            if (keep_data == TRUE)
            {
                if (num_chunks == keep_chunk)
                {
                    /* We are keeping the data */
                    memcpy(keep_buff, output, out_size);
                }
                
                num_chunks++;
            }
            
            stream.next_out = (Bytef *)output;
            stream.avail_out = out_size;
            
        }
        else if (status != Z_STREAM_END)
        {
            fprintf(stderr, "zlib error! %d, %s\n", status, stream.msg);
            status = inflateEnd(&stream);
            return 0;
        }
        
    }
    
    if (keep_data)
    {
        /* Copy the chunk we are keeping into the output buffer */
        if (num_chunks <= keep_chunk)
        {
            memcpy(keep_buff, output, out_size);  
        }
        memcpy(output, keep_buff, out_size);
    }

    size = stream.total_out;
    status = inflateEnd(&stream);
    
    return size;
    
}
                            
