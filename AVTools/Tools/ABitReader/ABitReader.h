/*
 * MpegtTS Basic Parser
 * Copyright (c) jeoliva, All rights reserved.

 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3.0 of the License, or (at your option) any later version.

 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.

 * You should have received a copy of the GNU Lesser General Public
 *License along with this library.
 */

#ifndef __A_BIT_READER_H__
#define __A_BIT_READER_H__

#include <sys/types.h>
#include <stdint.h>

typedef struct ABitReader {
    uint8_t *mData;          // 待读取数据地址
    size_t mSize;              // 剩余待读取数据大小
    uint32_t mReservoir;   // 32位的已读缓冲区
    size_t mNumBitsLeft;   // 已读缓冲区当前有效位数
} ABitReader;

void initABitReader(ABitReader *bitReader, uint8_t *data, size_t size);
uint32_t getBits(ABitReader *bitReader, size_t n);
void skipBits(ABitReader *bitReader, size_t n);
size_t numBitsLeft(ABitReader *bitReader);
uint8_t *getBitReaderData(ABitReader *bitReader);

#endif  // __A_BIT_READER_H_
