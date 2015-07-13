/* 
 *           DO WHAT THE FUCK YOU WANT TO PUBLIC LICENSE
 *                    Version 2, December 2004
 *  
 * Copyright (C) 2004 Sam Hocevar <sam@hocevar.net>
 * 
 * Everyone is permitted to copy and distribute verbatim or modified
 * copies of this license document, and changing it is allowed as long
 * as the name is changed.
 *  
 *            DO WHAT THE FUCK YOU WANT TO PUBLIC LICENSE
 *   TERMS AND CONDITIONS FOR COPYING, DISTRIBUTION AND MODIFICATION
 *  
 *  0. You just DO WHAT THE FUCK YOU WANT TO.
 */
/* 
 * Gai Flame script extractor
 * author : MooZ
 * katakana and hiragana table : Hiei
 */
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>

#define HEADER_SIZE 0x200
#define TABLE_OFFSET 0x2200
#define TABLE_ENTRY_COUNT 0x180

const uint8_t symbolTable[] =
{
    0x81,0x40, 0x82,0x4F, 0x00,0x63, 0x81,0x40, 0x81,0x41, 0x81,0x45, 0x83,0x92, 0x83,0x40, 0x83,0x42, 0x83,0x44, 0x83,0x46, 0x83,0x48, 0x83,0x83, 0x83,0x85, 0x83,0x87, 0x83,0x62, 
    0x81,0x5B, 0x83,0x41, 0x83,0x43, 0x83,0x45, 0x83,0x47, 0x83,0x49, 0x83,0x4A, 0x83,0x4C, 0x83,0x4E, 0x83,0x50, 0x83,0x52, 0x83,0x54, 0x83,0x56, 0x83,0x58, 0x83,0x5A, 0x83,0x5C, 
    0x83,0x5E, 0x83,0x60, 0x83,0x63, 0x83,0x65, 0x83,0x67, 0x83,0x69, 0x83,0x6A, 0x83,0x6B, 0x83,0x6C, 0x83,0x6D, 0x83,0x6E, 0x83,0x71, 0x83,0x74, 0x83,0x77, 0x83,0x7A, 0x83,0x7D, 
    0x83,0x7E, 0x83,0x80, 0x83,0x81, 0x83,0x82, 0x83,0x84, 0x83,0x86, 0x83,0x88, 0x83,0x89, 0x83,0x8A, 0x83,0x8B, 0x83,0x8C, 0x83,0x8D, 0x83,0x8F, 0x83,0x93, 0x81,0x4A, 0x81,0x4B, 
    0x81,0x5A, 0x81,0x42, 0x81,0x75, 0x81,0x76, 0x81,0x41, 0x81,0x45, 0x82,0xF0, 0x82,0x9F, 0x82,0xA1, 0x82,0xA3, 0x82,0xA5, 0x82,0xA7, 0x82,0xE1, 0x82,0xE3, 0x82,0xE5, 0x82,0xC1, 
    0x81,0x5B, 0x82,0xA0, 0x82,0xA2, 0x82,0xA4, 0x82,0xA6, 0x82,0xA8, 0x82,0xA9, 0x82,0xAB, 0x82,0xAD, 0x82,0xAF, 0x82,0xB1, 0x82,0xB3, 0x82,0xB5, 0x82,0xB7, 0x82,0xB9, 0x82,0xBB, 
    0x82,0xBD, 0x82,0xBF, 0x82,0xC2, 0x82,0xC4, 0x82,0xC6, 0x82,0xC8, 0x82,0xC9, 0x82,0xCA, 0x82,0xCB, 0x82,0xCC, 0x82,0xCD, 0x82,0xD0, 0x82,0xD3, 0x82,0xD6, 0x82,0xD9, 0x82,0xDC, 
    0x82,0xDD, 0x82,0xDE, 0x82,0xDF, 0x82,0xE0, 0x82,0xE2, 0x82,0xE4, 0x82,0xE6, 0x82,0xE7, 0x82,0xE8, 0x82,0xE9, 0x82,0xEA, 0x82,0xEB, 0x82,0xED, 0x82,0xF1, 0x81,0x4A, 0x81,0x4B
};

const off_t mpr[8] =
{
    0, 0, 0, 3, 4, 1, 0, 0 
};

int main(int argc, char** argv)
{
    FILE *input, *output;
    uint8_t table[TABLE_ENTRY_COUNT];
    uint8_t header[4], byte;
    size_t nRead;
    off_t addr;
    int i, err = 1;
        
    if(argc != 3)
    {
        fprintf(stderr, "Usage: %s rom.pce script.txt\n", argv[0]);
        return err;
    }

    input = fopen(argv[1], "rb");
    if(input == NULL)
    {
        fprintf(stderr, "Unable to open %s: %s.\n", argv[1], strerror(errno));
        return err;
    }
    
    /* Load pointer table. */
    fseek(input, TABLE_OFFSET, SEEK_SET);
    nRead = fread(table, 1, TABLE_ENTRY_COUNT, input);
    if(nRead != TABLE_ENTRY_COUNT)
    {
        fprintf(stderr, "An error occured while reading pointer table: %s.\n", strerror(errno));
        goto err_0;
    }
    
    /* Open output. */
    output = fopen(argv[2], "wb");
    if(output == NULL)
    {
        fprintf(stderr, "Unable to open %s: %s.\n", argv[2], strerror(errno));
        goto err_0;
    }
    
    /* Dump strings. */
    for(i=0; i<TABLE_ENTRY_COUNT; i+=2)
    {
        unsigned int delta;
        unsigned int symbol;
        int mprId;
        off_t offset;
        
        /* Compute rom offset and jump to it. */
        mprId = table[i+1] >> 5;
        offset = (table[i+1] & 0x1f) << 8;
        addr = table[i] + (mpr[mprId] << 13) + offset + HEADER_SIZE;
        fseek(input, addr, SEEK_SET);
        
        /* Read string header. */
        nRead = fread(header, 1, 4, input);
        if(nRead != 4)
        {
            fprintf(stderr, "An error occured while reading string header (%02x%02x): %s.\n", table[i+1], table[i], strerror(errno));
            goto err_1;
        }

        /* Output string infos (address, rom offset and header). */
        fprintf(output, "<string address=\"%02x%02x\" romOffset=\"%lx\" header=\"%02x%02x%02x%02x\">\n", 
               table[i+1], table[i], addr,
               header[0], header[1], header[2], header[3]);

        /* Decode string. */       
        delta = 0;
        while(fread(&byte, 1, 1, input))
        {
            symbol = byte;
            if(symbol == 0x00)
            {
                /* We stop decoding as soon as we read the end of string symbol (\0). */
                break;
            }
            else if(symbol == 0x7C)
            {
                /* We must add 0xC0 to each byte read between this "tags". */
                delta = (delta == 0xC0) ? 0x00 : 0xC0;
            }
            else
            {
                symbol = (byte + delta) & 0xff;
                if(symbol >= 0xf0)
                {
                    if(symbol == 0xff) /* newline */
                    {
                        fprintf(output, "<endl/>\n");
                    }
                    else /* The only other symbol found in the script is 0xfe. */
                    {
                        /* [todo] Figure out what 0xfe does. */
                        fprintf(output, "<symbol id=\"0x%02x\"/>", symbol);
                    }                        
                }
                else if((symbol >= 0x20) && (symbol <= 0x5f))
                {
                    /* Standard ASCII character. */
                    fprintf(output, "%c", symbol);
                }
                else
                {
                    symbol -= 0x60;
                    if((symbol <= 0) || (symbol == 64) || (symbol >= 128))
                    {
                        /* Tiles. */
                        fprintf(output, "<tile id=\"0x%02x\"/>", symbol+0x60); 
                    }
                    else
                    {
                        /* Katakana and hiragana. */
                        symbol *= 2;
                        if(symbolTable[symbol])
                        {
                            fwrite(symbolTable+symbol, 1, 1, output);
                        }
                        fwrite(symbolTable+symbol+1, 1, 1, output);
                    }
                }
            }
        }
        fprintf(output, "\n</string>\n");
    }

    err = 0;
err_1:
    fclose(output);
err_0:   
    fclose(input);    
    return err;
}
