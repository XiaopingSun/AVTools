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

#include "ABitReader.h"

/**
 * 初始化 ABitReader
 * @param bitReader       ABitReader Instance
 * @param data             待读取数据指针
 * @param size              待读取数据大小
 */
void initABitReader(ABitReader *bitReader, uint8_t *data, size_t size) {
    bitReader->mData = data;
    bitReader->mSize = size;
    bitReader->mReservoir = 0;
    bitReader->mNumBitsLeft = 0;
}

/**
 * 填充已读缓冲区
 * @param bitReader         ABitReader Instance
 */
void fillReservoir(ABitReader *bitReader) {
    // 已读缓冲区置0
	bitReader->mReservoir = 0;
    size_t i = 0;
    
    // 向已读缓冲区读入数据  尝试读取4字节
    for (i = 0; bitReader->mSize > 0 && i < 4; ++i) {
        // 每次读取前  先将mReservoir左移8位  将待读数据在内存低位的字节读到 mReservoir 的高位
        bitReader->mReservoir = (bitReader->mReservoir << 8) | *(bitReader->mData);
        // 待读取指针前移一字节
        ++bitReader->mData;
        // 待读取数据大小减一
        --bitReader->mSize;
    }
    // 记录已读缓冲区有效位数
    bitReader->mNumBitsLeft = 8 * i;
    // 如果4字节没读满  将剩余位数左移
    bitReader->mReservoir <<= 32 - bitReader->mNumBitsLeft;
}

/**
 * 已读缓冲区高位插入数据
 * @param bitReader         ABitReader Instance
 * @param x                    data
 * @param n                    bit count
 */
void putBits(ABitReader *bitReader, uint32_t x, size_t n) {
    
    bitReader->mReservoir = (bitReader->mReservoir >> n) | (x << (32 - n));
    bitReader->mNumBitsLeft += n;
}

/**
 * 从已读缓冲区读取n位
 * @param bitReader         ABitReader Instance
 * @param n                    bit count
 */
uint32_t getBits(ABitReader *bitReader, size_t n) {
    
    uint32_t result = 0;
    // 开始循环读取  直到n为0
    while (n > 0) {
        // 如果已读缓冲区没有数据  重新读取
        if (bitReader->mNumBitsLeft == 0) {
            fillReservoir(bitReader);
        }
        
        size_t m = n;
        // 如果已读缓冲区剩余位数小于需要的位数  则本次只读取已读缓冲区剩余大小  下次进入循环重新填充已读缓冲区
        if (m > bitReader->mNumBitsLeft) {
            m = bitReader->mNumBitsLeft;
        }
        // result先左移m位给本次读取留位置   mReservoir 右移将高位的m位对齐result的低位m位
        result = (result << m) | (bitReader->mReservoir >> (32 - m));
        // m位读取完成  将 mReservoir 左移m位 有效位数mNumBitsLeft减去m位
        bitReader->mReservoir <<= m;
        bitReader->mNumBitsLeft -= m;
        // n减去已读m位  如果还有剩余  进入下个循环
        n -= m;
    }
    return result;
}

/**
 * 从已读缓冲区跳过n位
 * @param bitReader         ABitReader Instance
 * @param n                    bit count
 */
void skipBits(ABitReader *bitReader, size_t n) {
    
    // bitReader->mReservoir 容量只有32位
	while (n > 32) {
        getBits(bitReader, 32);
        n -= 32;
    }
    if (n > 0) {
        getBits(bitReader, n);
    }
}

/**
 * 获取剩余数据大小
 * @param bitReader         ABitReader Instance
 * @return 待读取数据和已读缓冲区数据大小总和
 */
size_t numBitsLeft(ABitReader *bitReader) {
	return bitReader->mSize * 8 + bitReader->mNumBitsLeft;
}

/**
 * 获取当前数据在待读取数据中的指针位置
 * @param bitReader         ABitReader Instance
 * @return 返回已读缓冲区当前数据在待读取数据中的位置指针
 */
uint8_t *getBitReaderData(ABitReader *bitReader) {
	return bitReader->mData - bitReader->mNumBitsLeft / 8;
}
