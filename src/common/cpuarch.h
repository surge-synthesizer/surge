
// CPU architecture enums

enum cpu_architecture
{	
	ca_SSE		= 0x1,
	ca_SSE2		= 0x2,
	ca_SSE3		= 0x4,
	ca_3DNOW	= 0x8,
	ca_X64		= 0x10,
	ca_PPC		= 0x20,
	ca_CMOV		= 0x40,
};

unsigned int determine_support();