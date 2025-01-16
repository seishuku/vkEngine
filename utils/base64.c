#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const int8_t base64LUT[256]=
{
	['A']=0, ['B']=1, ['C']=2, ['D']=3, ['E']=4, ['F']=5, ['G']=6, ['H']=7, ['I']=8,
	['J']=9, ['K']=10,['L']=11,['M']=12,['N']=13,['O']=14,['P']=15,['Q']=16,['R']=17,
	['S']=18,['T']=19,['U']=20,['V']=21,['W']=22,['X']=23,['Y']=24,['Z']=25,['a']=26,
	['b']=27,['c']=28,['d']=29,['e']=30,['f']=31,['g']=32,['h']=33,['i']=34,['j']=35,
	['k']=36,['l']=37,['m']=38,['n']=39,['o']=40,['p']=41,['q']=42,['r']=43,['s']=44,
	['t']=45,['u']=46,['v']=47,['w']=48,['x']=49,['y']=50,['z']=51,['0']=52,['1']=53,
	['2']=54,['3']=55,['4']=56,['5']=57,['6']=58,['7']=59,['8']=60,['9']=61,['+']=62,
	['/']=63,
};

size_t base64Decode(const char *input, uint8_t *output)
{
	size_t inputLength=strlen(input), outputLength=0;

	if(inputLength%4!=0)
	{
		fprintf(stderr, "Invalid Base64 input length.\n");
		return 0;
	}

	for(size_t i=0;i<inputLength; i+=4)
	{
		uint32_t chunk=0;

		// Map characters to 6-bit values
		for(int j=0; j<4; j++)
		{
			char c=input[i+j];
			if(c=='=')
				chunk<<=6;  // Add padding zeros for '='
			else
			{
				int8_t value=base64LUT[(unsigned char)c];
				if(value==-1)
				{
					fprintf(stderr, "Invalid Base64 character: %c\n", c);
					return 0;
				}
				chunk=(chunk<<6)|value;
			}
		}

		// Convert 24-bit chunk into three 8-bit bytes
		output[outputLength++]=(chunk>>16)&0xFF;

		if(input[i+2]!='=')
			output[outputLength++]=(chunk>>8)&0xFF;

		if(input[i+3]!='=')
			output[outputLength++]=chunk&0xFF;
	}

	return outputLength;
}
