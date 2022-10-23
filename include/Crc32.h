#ifndef Crc32
#define Crc32

#include <stdint.h>

extern "C" 
{
	//Crc32校验值计算处理函数
	uint32_t crc32(const unsigned char* s, unsigned int len);

	//Crc32多项式校验函数
	uint32_t crc32_poly(uint32_t poly, uint16_t value);
}

#endif // !Crc32

