/*
 * guyframe.c
 *
 * This is a script extractor for Guy Frame
 * 
 */


#include <stdio.h>
#include <stdlib.h>


#define	FALSE			0
#define	TRUE			1


#define DEBUG			1
#define VERBOSE			1

#define DATA_ASCII		0
#define DATA_UNICODE	1

#define BUFFER_REDLINE	0x100


/* global ROM area */
unsigned char rom[1048576];
int rom_size;

/* global arrays for thingy and wordcount */
unsigned char thingy_array[512][256];
char buffer_out[50000];
int  bufout_ind;
char buffer_temp[5000];
int  buftemp_ind;
unsigned char buffer[50000];


/* admittedly, this is a bit sloppy */
/* this is shared stuff for multiple-iterations */
/* of calling the script-block translator */
/* some should be static variables, and some should be public */
/* and some should be passed back.  But I'm too lazy */
FILE *fdump, *fword;
int  script_line;
int  pointerloc;
int  english_length;
int  blockstart, blockend, blocksize;
int  numphrases, superblockstart;
int  default_blockstart, newblockstart;
int  newblock_offset, newblock_hi, newblock_lo, newblock_bank;
char * pindex;
int  str_index, found, pointerlen;


/* forward-reference declarations for functions */
int   thingy_index(char *buf);
int   thingy_index_bybrace(char *buf);
int   readline(FILE *fin, char *buffer, int datatype);
int   num_param(char *in, int *out, int radix, int len);
int   implant_intval(char *out, int input, int num_bytes);
void  hexdump(char *in, int count);
int   find_substr(char *outer_block, int outer_len, char *inner_block, int inner_len);
int   extract_data(FILE * fscript, int base);


int
main(int argc, char ** argv)
{
/* internal stuff */
FILE *fthingy, *fscript, *fromfile;
int  thingy_line;
int i, j, k, l;

/* command-line parameters */
char thingy_name[256];
char script_name[256];
char romfile_name[256];


	default_blockstart = 0x02200;

	/* initialize thingy array */
	for (i = 0; i < 512; i++)
	{
		memset(&thingy_array[i][0], 0, 512);
	}

#ifdef DEBUG
	/* print list of command-line parameters */
	for (i = 0; i < argc; i++)
	{
		printf("argv[%d] = %s\n", i, argv[i]);
	}
	printf("\n");
#endif

    if (argc != 4)
	{
		printf("Invalid number of parameters : %d\n", argc);
		printf("Usage:\n------\n");
		printf("guyframe <thingy_file> <script_file> <rom_file>\n\n");
		printf("(Currently doesn't update ROM file)\n");
		exit(1);
	}

	strcpy(thingy_name, argv[1]);
	strcpy(script_name, argv[2]);
	strcpy(romfile_name, argv[3]);

#ifdef DEBUG
	printf("Values:\n");
	printf("-------\n");
	printf("thingy file  = %s\n", thingy_name);
	printf("script file  = %s\n", script_name);
	printf("ROM file     = %s\n", romfile_name);
	printf("\n");
#endif

	/* open thingy file */
	fthingy = fopen(thingy_name, "rb");
	if (!fthingy)
	{
		printf("Couldn't open Thingy file %s\n", thingy_name);
		exit(1);
	}

	/* open script file */
	fscript = fopen(script_name, "wb");
	if (!fscript)
	{
		printf("Couldn't open Script file %s\n", script_name);
		exit(1);
	}

	/* open rom file */
	fromfile = fopen(romfile_name, "rb");
	if (!fromfile)
	{
		printf("Couldn't open ROM file %s\n", romfile_name);
		exit(1);
	}

	/*****************/
	/* read ROM file */
	/*****************/
	rom_size = 0;
	while (!feof(fromfile))
	{
		rom[rom_size++] = fgetc(fromfile);
	}
	rom_size--;
	fclose(fromfile);

	printf("ROM size = %d\n", rom_size);


	/********************/
	/* read thingy file */
	/********************/
	thingy_line = 1;

	while (!feof(fthingy))
	{
		memset(&buffer[0], 0, sizeof(buffer));

		readline(fthingy, buffer, DATA_ASCII);

		if (buffer[0] == '\0')
			break;

		if (num_param(&buffer[0], &str_index, 16, 3) != 0)
		{
			printf("error understanding thingy_line #%d:\n%s\n",
							thingy_line, buffer);
			exit(1);
		}

		if (buffer[3] != '=')
		{
			printf("error: '=' should be 4th position on thingy_line #%d:\n%s\n",
							thingy_line, buffer);
			exit(1);
		}

#ifdef DEBUG
		/*
		printf("thingy_line #%d, str_index #%X, len = %d; values = ",
				thingy_line, str_index, strlen(&buffer[3]) );
		for (i = 0; i < strlen(&buffer[3]); i++)
		{
			printf("%2.2X ", buffer[3 + i]);
		}
		printf("\n");
		*/
#endif
		strcpy(&thingy_array[str_index][0], &buffer[4]);
		thingy_line++;
	}
	printf("\n");
	
	fclose(fthingy);

	/*********************/
	/* write script file */
	/*********************/
	i = 0x2200;
	while (i < 0x2380)
	{
		extract_data(fscript, i);
		i += 2;
	}
	fclose(fscript);


	/* free buffers */

	exit(0);
}


int
extract_data(FILE * fscript, int base)
{
int ptr_addr, ptr_addr_in_rom;
int hdr1, hdr2, hdr3, hdr4;
int byte;
int i;
int xcount, ycount;
int xmax, ymax;
int done;
int thingy_base = 0;
int translateable;

	translateable = TRUE;

	ptr_addr        = rom[base] + (rom[base+1] << 8);

	if ((ptr_addr >= 0xA000) && (ptr_addr <= 0xBFFF))
	{
		ptr_addr_in_rom = ptr_addr - 0x8000 + 0x200;
	}
	else if ((ptr_addr >= 0x6000) && (ptr_addr <= 0x7FFF))
	{
		ptr_addr_in_rom = ptr_addr + 0x200;
	}
	else
	{
		translateable = FALSE;
		ptr_addr_in_rom = 0x00;
	}

	hdr1 = rom[ptr_addr_in_rom];
	hdr2 = rom[ptr_addr_in_rom+1];
	hdr3 = rom[ptr_addr_in_rom+2];
	hdr4 = rom[ptr_addr_in_rom+3];

	fprintf(fscript, "0x%4.4x : 0x%4.4x (0x%4.4x)\n", base, ptr_addr, ptr_addr_in_rom);
	fprintf(fscript, "posy =0x%2.2x, posx =0x%2.2x\n", hdr1, hdr2);
	fprintf(fscript, "sizex=0x%2.2x, sizey=0x%2.2x\n", hdr3, hdr4);

	if (translateable == FALSE)
	{
		fprintf(fscript, "(cannot translate address)\n\n");
		return(0);
	}

	xmax = hdr3;
	ymax = hdr4;
	done = FALSE;
	i = 4;

	xcount = 0;
	ycount = 0;


	while ((done == FALSE) && (rom[ptr_addr_in_rom + i] != 0))
	{
		if (ycount == ymax)
		{
			fprintf(fscript, "[pagebreak]\n");
		}
		ycount = 0;

		while ((done == FALSE) && (ycount < ymax))
		{
			if (xcount == xmax)
			{
				ycount++;
				fprintf(fscript, "\n");
			}
			xcount = 0;

			while ((done == FALSE) && (ycount < ymax) && (xcount < xmax))
			{
				byte = rom[ptr_addr_in_rom + i++];
				if (byte == 0)
				{
					done = TRUE;
					break;
				}
				if (byte == '|')
				{
					thingy_base ^= 0x100;
					continue;
				}
				if (byte == 0xFF)
				{
					xcount = xmax;
					break;
				}
				if (byte == 0xFE)
				{
					xcount = xmax;
					ycount = ymax-1;
					break;
				}

				byte += thingy_base;

				fprintf(fscript, "%s", thingy_array[byte]);
				xcount++;
			}
		}
	}
	
    fprintf(fscript, "[END OF MSG]\n\n");
}

int
read_script_block(FILE *fscript)
{
FILE *fout;
int i,j,k,l;
int retval;

	/* first line is blank */
	retval = readline(fscript, buffer, DATA_ASCII);
	if (retval == EOF)
		return(retval);

	script_line++;

	/* second line is block ID */
	readline(fscript, buffer, DATA_ASCII);
	script_line++;
	printf("line #2 = %s\n\n", buffer);

	/* third line is memory locations */
	readline(fscript, buffer, DATA_ASCII);
	script_line++;

	printf("line #3 = \n%s\n", buffer);

	pindex = strstr(buffer, "-");
	if (pindex == NULL)
	{
		printf("Couldn't find '-' in block location line\n");
		exit(1);
	}

	str_index = ((void*)pindex - (void*)buffer);
	
	printf("#3: str_index = %d, strlen = %d \n", str_index, strlen(buffer));

	num_param(&buffer[0], &blockstart, 0, str_index);
	num_param(&buffer[str_index+1], &blockend, 0, strlen(buffer)-str_index-1);
	blocksize = blockend - blockstart + 1;

	printf("(blockstart = %x, blockend = %x, length = %x (%d)\n\n",
					blockstart, blockend, blocksize, blocksize);

	/***********************************/
	/* fourth line is memory locations */
	/***********************************/
	readline(fscript, buffer, DATA_ASCII);
	script_line++;

	printf("line #4 = \n%s\n", buffer);

	pindex = strstr(buffer, "@");
	if (pindex == NULL)
	{
		printf("Couldn't find '@' in block location line\n");
		exit(1);
	}

	str_index = ((void*)pindex - (void*)buffer);
	
	printf("line #4: str_index = %d, strlen = %d \n", str_index, strlen(buffer));

	num_param(&buffer[0], &numphrases, 0, str_index);
	num_param(&buffer[str_index+1], &superblockstart, 0, strlen(buffer)-str_index-1);

	printf("(num_phrases = %x, blockstart = %x\n\n", numphrases, superblockstart);

	/*************************************/
	/* fifth line is new memory location */
	/*************************************/
	readline(fscript, buffer, DATA_ASCII);
	script_line++;

	printf("line #5 = \n%s\n", buffer);

	pindex = strstr(buffer, "@");
	if (pindex == NULL)
	{
		printf("Couldn't find '@' in new start location line\n");
		exit(1);
	}

	str_index = ((void*)pindex - (void*)buffer);
	
	printf("line #5: str_index = %d, strlen = %d \n", str_index, strlen(buffer));

	if (strcmp(&buffer[str_index+1], "++") == 0)
	{
		newblockstart = default_blockstart;
	}
	else
	{
		num_param(&buffer[str_index+1], &newblockstart, 0, strlen(buffer)-str_index-1);
	}

	/* when calculating pointers, forget about 0x200 header size */
	newblock_bank = ((newblockstart-0x200) / 0x4000) * 2;
	newblock_offset = (newblockstart-0x200) - (newblock_bank * 0x2000) + 0x4000;
	newblock_lo = newblock_offset & 0xff;
	newblock_hi = (newblock_offset >> 8);

	printf("(newblockstart = %x, newblock ptrs = %2.2x, %2.2x, %2.2x\n\n",
					newblockstart, newblock_bank, newblock_lo, newblock_hi);

	/***********************************/
	/* sixth line is pointers to alter */
	/***********************************/
	readline(fscript, buffer, DATA_ASCII);
	script_line++;

	printf("line #6 = \n%s\n", buffer);

	pindex = strstr(buffer, "@");
	if (pindex == NULL)
	{
		printf("Couldn't find '@' in pointers line\n");
		exit(1);
	}

	str_index = ((void*)pindex - (void*)buffer);
	
	printf("line #6: str_index = %d, strlen = %d \n", str_index, strlen(buffer));

	if ((strlen(buffer)-str_index-1) != 0)
	{
		while (1)
		{
			pointerlen = strlen(buffer)-str_index-1;
			pindex = strstr(&buffer[str_index+1], ",");
			if (pindex != NULL)
			{
				pointerlen = ((void*)pindex - (void*)&buffer[str_index+1]);
			}
			num_param(&buffer[str_index+1], &pointerloc, 0, pointerlen);
			printf("pointerloc = %x\n", pointerloc);

			rom[pointerloc+0] = newblock_bank;
			rom[pointerloc+1] = newblock_lo;
			rom[pointerloc+2] = newblock_hi;

			if (pindex == NULL)
				break;
	
			str_index = ((void*)pindex - (void*)buffer);
		}
	}

	printf("\n");

	/********************/
	/* start processing */
	/********************/
	i = 0;
	english_length = 0;

	implant_intval(&buffer_out[0], numphrases, 2);

	bufout_ind = (numphrases+1) * 2;
	
	/* i counts phrases */

	while (!feof(fscript) && (i < numphrases))
	{
		
		implant_intval(&buffer_out[(i+1)*2], bufout_ind, 2);

		/* first line is blank */
		readline(fscript, buffer, DATA_ASCII);
		if (strlen(buffer) > 0)
		{
			printf("error reading script - line #%d should be blank\n", script_line);
			printf("%s\n", buffer);
			exit(1);
		}
		script_line++;

		/* second line is Japanese */
		readline(fscript, buffer, DATA_ASCII);
		script_line++;

		/* third line is English */
		readline(fscript, buffer, DATA_ASCII);
		script_line++;

		english_length += strlen(buffer);

		/* if no phrase, at least compress by   */
		/* re-using the last string termintator */
		if (strcmp(buffer, "{00}") == 0)
		{
			printf("found empty line: line #%d\n", script_line);
			i++;
			continue;
		}



		/* translate from english input buffer to encoded temp */
		/* buffer using thingy file as magic decoder ring */

		/* j counts offset within english phrase */
		j = 0;
		buftemp_ind = 0;
		while (buffer[j] != '\0')
		{
			if ((k = thingy_index(&buffer[j])) == -1)
			{
				if ((k = thingy_index_bybrace(&buffer[j])) == -1)
				{
					printf("Couldn't find a thingy for line #%d, offset %d, '%s'\n",
							script_line, j, &buffer[0]);
					exit(1);
				}
				else
				{
					buffer_temp[buftemp_ind] = k;
					buftemp_ind++;
					j += 4;
				}
			}
			else
			{
				buffer_temp[buftemp_ind] = k;
				buftemp_ind++;
				l = strlen(&thingy_array[k][0]);
				if (l > 0)
					j += l;
			}
		}

		/* try to find it as a substring of something already encoded */
		j = find_substr(&buffer_out[0], bufout_ind, &buffer_temp[0], buftemp_ind);

		if (j >= 0)
		{
			printf("Found line #%d as substring at %x ('%s')\n", script_line, j, buffer);
			implant_intval(&buffer_out[(i+1)*2], j, 2);
		}
		else
		{
			memcpy(&buffer_out[bufout_ind], &buffer_temp[0], buftemp_ind);
			bufout_ind += buftemp_ind;
		}

		i++;
	}

	printf("Original length = %d\n", blocksize);
	printf("Eng text length = %d\n", english_length);
	printf("Strings  length = %d\n", bufout_ind - ((numphrases+1) *2) );


#ifdef DEBUG
	/*
	hexdump(&buffer_out[0], 0x200);
	*/

	fdump = fopen("output.bin", "wb");
	if (fdump == NULL)
	{
		printf("couldn't open fdump file\n");
		exit(1);
	}
	for (i = 0; i < bufout_ind; i++)
	{
		fputc(buffer_out[i], fdump);
	}
	fclose(fdump);
#endif

	/* implant text into ROM image */
	for (i = 0; i < bufout_ind; i++)
	{
		rom[newblockstart + i] = buffer_out[i];
	}
	default_blockstart = newblockstart + i;

}


int
thingy_index(char *buf)
{
int i, j;

	/* try tokenized strings first */
	for (i = 0xc0; i <= 0xff; i++)
	{
		j = strlen(&thingy_array[i][0]);

		if (j == 0)
			continue;

		if (memcmp(buf, &thingy_array[i][0], j) == 0)
			return(i);
	}

	/* next, try tokenized control sequences */
	for (i = 0x00; i <= 0x1f; i++)
	{
		j = strlen(&thingy_array[i][0]);

		if (j == 0)
			continue;

		if (memcmp(buf, &thingy_array[i][0], j) == 0)
			return(i);
	}

	/* next, try regular sequences */
	for (i = 0x20; i <= 0xbf; i++)
	{
		j = strlen(&thingy_array[i][0]);

		if (j == 0)
			continue;

		if (memcmp(buf, &thingy_array[i][0], j) == 0)
			return(i);
	}

	/* else, return -1 if not found at all */
	return(-1);
}


int
thingy_index_bybrace(char *buf)
{
int i,j;

	if ( (*buf == '{') &&
	     (*(buf+3) == '}') )
	{
		i = num_param( (buf+1), &j, 16, 2);
		if (i != -1)
		{
			return(j);
		}	
	}

	return(-1);
}


int
readline(FILE *fin, char *buf, int datatype)
{
int  c, c1, c2, i;

	i = 0;

	while (1)
	{
		c = fgetc(fin);

		if (c == EOF)
			break;

		if (datatype == DATA_UNICODE)
		{
			c1 = c;
			c2 = fgetc(fin);

			if (c2 == EOF)
				break;

			c1 &= 0xff;
			c2 &= 0xff;
			
			c = (c2 << 8) | c1;
		}

		if (c == 0x0D)
			continue;
		if (c == 0x0A)
			break;

		/* I can only do this because I am ignoring the */
		/* non-ASCII information in the UNICODE file    */
		buf[i++] = (char)c;
	}

	buf[i] = '\0';

	return(c);
}


int
implant_intval(char *out, int input, int num_bytes)
{
int i;
int temp = input;

	for (i = 0; i < num_bytes; i++)
	{
		*(out + i) = (char) (temp & 0xff);
		temp >>= 8;
	}
}


int
num_param(char *in, int *out, int radix, int len)
{
int i, ret;
int currval, tempint;
char temp;

	i = 0;
	ret = 0;
	currval = 0;

	/* if len = 0, then it's limitless */
	if (len == 0)
		len = 65535;

	/* default radix to 10 */
	if (radix == 0)
		radix = 10;

	/* if '0x' found in input stream, force override to radix of 16 */
	if (strncmp(in, "0x", 2) == 0)
	{
		radix = 16;
		i = 2;
	}

	while ( (*(in+i) != 0) && (i < len))
	{
		temp = *(in+i);

		if ((temp >= '0') && (temp <= '9'))
		{
			tempint = (int) (temp - '0');
		}
		else if (radix == 16)
		{
			if ((temp >= 'A') && (temp <= 'F'))
			{
				tempint = (int) (temp - 'A') + 10;
			}
			else if ((temp >= 'a') && (temp <= 'f'))
			{
				tempint = (int) (temp - 'a') + 10;
			}
		}
		else
		{
			*out = currval;
			return(1);
		}

		currval *= radix;
		currval += tempint;
		i++;
	}
			
	*out = currval;
	return(0);
}

void
hexdump(char *in, int count)
{
int i, j;
unsigned char c;

	for (i = 0; i < count; i += 16)
	{
		printf("%4.4X: ", i);

		for (j = 0; j < 16; j++)
		{
			c = *(in+i+j);
			printf("%2.2X ", c);
		}

		printf("\n");
	}
}

int
find_substr(char *outer_block, int outer_len, char *inner_block, int inner_len)
{
int	i, j;

	for (i = 0; i < (outer_len - inner_len + 1); i++)
	{
		if (memcmp((outer_block+i), inner_block, inner_len) == 0)
		{
			return(i);
		}
	}

	return(-1);
}
