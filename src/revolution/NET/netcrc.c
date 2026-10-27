#include <revolution/NET.h>

// CRC-32 (reflected polynomial 0xEDB88320), four bits at a time
static const u32 sCrcTable[16] = {
    0x00000000, 0x1DB71064, 0x3B6E20C8, 0x26D930AC, 0x76DC4190, 0x6B6B51F4, 0x4DB26158, 0x5005713C,
    0xEDB88320, 0xF00F9344, 0xD6D6A3E8, 0xCB61B38C, 0x9B64C2B0, 0x86D3D2D4, 0xA00AE278, 0xBDBDF21C,
};

u32 NETCalcCRC32(const void* data, u32 size) {
    const u8* p = (const u8*)data;
    u32 crc = 0xFFFFFFFF;
    u32 b;

    while (size-- != 0) {
        b = *p;
        p++;
        crc = (crc >> 4) ^ sCrcTable[(crc ^ b) & 0xF];
        crc = (crc >> 4) ^ sCrcTable[(crc ^ (b >> 4)) & 0xF];
    }

    return ~crc;
}
