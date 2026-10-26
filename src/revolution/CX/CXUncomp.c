#include <revolution/CX.h>

// This unit holds both CXStreamingUncompression.c and CXUncompression.c of
// the SDK (in that order).

void CXInitUncompContextLZ(CXUncompContextLZ* context, void* dest) {
    context->destp = (u8*)dest;
    context->destCount = 0;
    context->flags = 0;
    context->flagIndex = 0;
    context->length = 0;
    context->lengthFlg = 3;
    context->headerSize = 8;
    context->exFormat = 0;
    context->forceDestCount = 0;
}

void CXInitUncompContextHuffman(CXUncompContextHuffman* context, void* dest) {
    context->destp = (u8*)dest;
    context->destCount = 0;
    context->bitSize = 0;
    context->treeSize = -1;
    context->treep = &context->tree[0];
    context->destTmp = 0;
    context->destTmpCnt = 0;
    context->srcTmp = 0;
    context->srcTmpCnt = 0;
    context->headerSize = 8;
    context->forceDestCount = 0;
}

static inline u32 CXiReadHeader(u8* headerSize, s32* destCount, const u8* srcp, u32 srclen, s32 forceDestSize) {
    u32 readLen = 0;

    while (*headerSize > 0) {
        --*headerSize;

        if (*headerSize <= 3) {
            *destCount |= (*srcp << ((3 - *headerSize) * 8));
        } else if (*headerSize <= 6) {
            *destCount |= (*srcp << ((6 - *headerSize) * 8));
        }

        srcp++;
        readLen++;

        if (*headerSize == 4 && *destCount > 0) {
            *headerSize = 0;
        }

        if (--srclen == 0 && *headerSize > 0) {
            return readLen;
        }
    }

    if (forceDestSize > 0 && forceDestSize < *destCount) {
        *destCount = forceDestSize;
    }

    return readLen;
}

s32 CXReadUncompLZ(CXUncompContextLZ* context, const void* data, u32 len) {
    const u8* srcp = (const u8*)data;
    s32 dispLen;

    if (context->headerSize > 0) {
        u32 readLen;

        if (context->headerSize == 8) {
            if ((*srcp & 0xF0) != 0x10) {
                return CX_ERR_UNSUPPORTED;
            }

            context->exFormat = (u8)(*srcp & 0x0F);
            if (context->exFormat != 0 && context->exFormat != 1) {
                return CX_ERR_UNSUPPORTED;
            }
        }

        readLen = CXiReadHeader(&context->headerSize, &context->destCount, srcp, len, context->forceDestCount);
        srcp += readLen;
        len -= readLen;

        if (len == 0) {
            return (context->headerSize == 0) ? context->destCount : CX_ERR_UNSUPPORTED;
        }
    }

    while (context->destCount > 0) {
        while (context->flagIndex > 0) {
            if (len == 0) {
                return context->destCount;
            }

            if (!(context->flags & 0x80)) {
                *context->destp++ = *srcp++;
                context->destCount--;
                len--;
            } else {
                while (context->lengthFlg > 0) {
                    --context->lengthFlg;

                    if (!context->exFormat) {
                        context->length = *srcp++;
                        context->length += (3 << 4);
                        context->lengthFlg = 0;
                    } else {
                        switch (context->lengthFlg) {
                        case 2:
                            context->length = *srcp++;

                            if ((context->length >> 4) == 1) {
                                context->length = (context->length & 0x0F) << 16;
                                context->length += ((0xFF + 0xF + 3) << 4);
                            } else if ((context->length >> 4) == 0) {
                                context->length = (context->length & 0x0F) << 8;
                                context->length += ((0xF + 2) << 4);
                                context->lengthFlg = 1;
                            } else {
                                context->length += (1 << 4);
                                context->lengthFlg = 0;
                            }
                            break;
                        case 1:
                            context->length += (*srcp++ << 8);
                            break;
                        case 0:
                            context->length += *srcp++;
                            break;
                        }
                    }

                    if (--len == 0) {
                        return context->destCount;
                    }
                }

                dispLen = (context->length & 0x0F) << 8;
                context->length >>= 4;
                dispLen = (dispLen | *srcp++) + 1;
                len--;
                context->lengthFlg = 3;

                if (context->length > context->destCount) {
                    if (context->forceDestCount == 0) {
                        return CX_ERR_DEST_OVERRUN;
                    }

                    context->length = context->destCount;
                }

                while (context->length > 0) {
                    *context->destp = context->destp[-dispLen];
                    context->destp++;
                    context->destCount--;
                    context->length--;
                }
            }

            if (context->destCount == 0) {
                goto out;
            }

            context->flags <<= 1;
            context->flagIndex--;
        }

        if (len == 0) {
            return context->destCount;
        }

        context->flags = *srcp++;
        context->flagIndex = 8;
        len--;
    }

out:
    if (context->forceDestCount == 0 && len > 32) {
        return CX_ERR_SRC_REMAINDER;
    }

    return CX_ERR_SUCCESS;
}

// A node holds the offset to its pair of children in the low 6 bits; bits 7
// and 6 mark the left and right child as leaves.
static inline u8* GetNextNode(const u8* treep, u32 select) {
    return (u8*)((((u32)treep >> 1) << 1) + (((*treep & 0x3F) + 1) << 1) + select);
}

s32 CXReadUncompHuffman(CXUncompContextHuffman* context, const void* data, u32 len) {
    const u8* srcp = (const u8*)data;

    if (context->headerSize > 0) {
        u32 readLen;

        if (context->headerSize == 8) {
            context->bitSize = (u8)(*srcp & 0x0F);

            if ((*srcp & 0xF0) != 0x20) {
                return CX_ERR_UNSUPPORTED;
            }

            if (context->bitSize != 4 && context->bitSize != 8) {
                return CX_ERR_UNSUPPORTED;
            }
        }

        readLen = CXiReadHeader(&context->headerSize, &context->destCount, srcp, len, context->forceDestCount);
        srcp += readLen;
        len -= readLen;

        if (len == 0) {
            return (context->headerSize == 0) ? context->destCount : CX_ERR_UNSUPPORTED;
        }
    }

    if (context->treeSize < 0) {
        context->treeSize = (s16)((*srcp + 1) * 2 - 1);
        *context->treep++ = *srcp++;
        len--;
    }

    while (context->treeSize > 0) {
        if (len == 0) {
            return context->destCount;
        }

        *context->treep++ = *srcp++;
        context->treeSize--;
        len--;

        if (context->treeSize == 0) {
            context->treep = &context->tree[1];

            if (!CXiVerifyHuffmanTable_(&context->tree[0], context->bitSize)) {
                return CX_ERR_ILLEGAL_TABLE;
            }
        }
    }

    while (context->destCount > 0) {
        while (context->srcTmpCnt < 32) {
            if (len == 0) {
                return context->destCount;
            }

            context->srcTmp |= (*srcp++) << context->srcTmpCnt;
            len--;
            context->srcTmpCnt += 8;
        }

        while (context->srcTmpCnt > 0) {
            u32 treeShift = context->srcTmp >> 31;
            u32 isLeaf = (*context->treep << treeShift) & 0x80;
            context->treep = GetNextNode(context->treep, treeShift);
            context->srcTmp <<= 1;
            context->srcTmpCnt--;

            if (isLeaf) {
                context->destTmp >>= context->bitSize;
                context->destTmp |= *context->treep << (32 - context->bitSize);
                context->treep = &context->tree[1];
                context->destTmpCnt += context->bitSize;

                if (context->destCount <= (context->destTmpCnt / 8)) {
                    context->destTmp >>= (32 - context->destTmpCnt);
                    context->destTmpCnt = 32;
                }

                if (context->destTmpCnt == 32) {
                    *(u32*)context->destp = CXiConvertEndian_(context->destTmp);
                    context->destp += 4;
                    context->destCount -= 4;
                    context->destTmpCnt = 0;

                    if (context->destCount <= 0) {
                        goto out;
                    }
                }
            }
        }
    }

out:
    if (context->forceDestCount == 0 && len > 32) {
        return CX_ERR_SRC_REMAINDER;
    }

    return CX_ERR_SUCCESS;
}

u32 CXGetUncompressedSize(const void* srcp) {
    u32 size = CXiConvertEndian_(*(u32*)srcp) >> 8;

    if (size == 0) {
        size = CXiConvertEndian_(*((u32*)srcp + 1));
    }

    return size;
}

void CXUncompressLZ(const void* srcp, void* destp) {
    const u8* pSrc = (const u8*)srcp;
    u8* pDst = (u8*)destp;
    BOOL exFormat;
    u32 destCount = CXiConvertEndian_(*(u32*)pSrc) >> 8;
    exFormat = (*pSrc & 0x0F) ? TRUE : FALSE;

    pSrc += 4;

    if (destCount == 0) {
        destCount = CXiConvertEndian_(*(u32*)pSrc);
        pSrc += 4;
    }

    while (destCount > 0) {
        u32 i;
        u32 flags = *pSrc++;

        for (i = 0; i < 8; i++) {
            if (!(flags & 0x80)) {
                *pDst++ = *pSrc++;
                destCount--;
            } else {
                s32 length = *pSrc >> 4;
                s32 offset;

                if (!exFormat) {
                    length += 3;
                } else if (length == 1) {
                    length = (*pSrc++ & 0x0F) << 12;
                    length |= (*pSrc++) << 4;
                    length |= (*pSrc >> 4);
                    length += 0xFF + 0xF + 3;
                } else if (length == 0) {
                    length = (*pSrc++ & 0x0F) << 4;
                    length |= (*pSrc >> 4);
                    length += 0xF + 2;
                } else {
                    length += 1;
                }

                offset = (*pSrc++ & 0x0F) << 8;
                offset = (offset | *pSrc++) + 1;

                if (length > destCount) {
                    length = destCount;
                }

                destCount -= length;

                do {
                    *pDst = pDst[-offset];
                    pDst++;
                } while (--length > 0);
            }

            if (destCount == 0) {
                break;
            }

            flags <<= 1;
        }
    }
}

void CXUncompressHuffman(const void* srcp, void* destp) {
    const u32* pSrc = (const u32*)srcp;
    u32* pDst = (u32*)destp;
    s32 destCount = CXiConvertEndian_(*pSrc) >> 8;
    u8* treep = (destCount != 0) ? ((u8*)pSrc + 4) : ((u8*)pSrc + 8);
    u8* treeStartp = treep + 1;
    u32 dataBit = *(u8*)pSrc & 0x0F;
    u32 destTmp = 0;
    u32 destTmpCount = 0;
    u32 destTmpDataNum = 4 + (*(u8*)pSrc & 0x7);

    if (destCount == 0) {
        destCount = CXiConvertEndian_(*(pSrc + 1));
    }

    pSrc = (u32*)(treep + ((*treep + 1) << 1));
    treep = treeStartp;

    while (destCount > 0) {
        s32 srcCount = 32;
        u32 srcTmp = CXiConvertEndian_(*pSrc++);

        while (--srcCount >= 0) {
            u32 treeShift = srcTmp >> 31;
            u32 treeCheck = *treep;

            treeCheck <<= treeShift;
            treep = (u8*)((((u32)treep >> 1) << 1) + (((*treep & 0x3F) + 1) << 1) + treeShift);

            if (treeCheck & 0x80) {
                destTmp >>= dataBit;
                destTmp |= *treep << (32 - dataBit);
                treep = treeStartp;

                if (++destTmpCount == destTmpDataNum) {
                    *pDst++ = CXiConvertEndian_(destTmp);
                    destCount -= 4;
                    destTmpCount = 0;
                }
            }

            if (destCount <= 0) {
                break;
            }

            srcTmp <<= 1;
        }
    }
}

BOOL CXiVerifyHuffmanTable_(const void* table, u8 bit) {
    const u32 FLAGS_ARRAY_NUM = 512 / 8;
    u8* treep = (u8*)table;
    u8* treeStartp = treep + 1;
    u32 treeSize = *treep;
    u8* treeEndp = (u8*)table + (treeSize + 1) * 2;
    u32 i;
    u8 end_flags[512 / 8];
    u32 idx;

    for (i = 0; i < FLAGS_ARRAY_NUM; i++) {
        end_flags[i] = 0;
    }

    if (bit == 4) {
        if (treeSize >= 0x10) {
            return FALSE;
        }
    }

    idx = 1;
    treep = treeStartp;

    while (treep < treeEndp) {
        if ((end_flags[idx / 8] & (1 << (idx % 8))) == 0) {
            u32 offset = (u32)(((*treep & 0x3F) + 1) << 1);
            u8* nodep = (u8*)((((u32)treep >> 1) << 1) + offset);

            if (*treep == 0 && idx >= (treeSize * 2)) {
                goto next;
            }

            if (nodep >= treeEndp) {
                return FALSE;
            }

            if (*treep & 0x80) {
                u32 left = (idx & ~0x1) + offset;
                end_flags[left / 8] |= (u8)(1 << (left % 8));
            }

            if (*treep & 0x40) {
                u32 right = (idx & ~0x1) + offset + 1;
                end_flags[right / 8] |= (u8)(1 << (right % 8));
            }
        }
    next:
        ++idx;
        ++treep;
    }

    return TRUE;
}
