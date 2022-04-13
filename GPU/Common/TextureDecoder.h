// Copyright (c) 2012- PPSSPP Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0 or later versions.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official git repository and contact information can be found at
// https://github.com/hrydgard/ppsspp and http://www.ppsspp.org/.

#pragma once

#include "ppsspp_config.h"

#include "Common/Common.h"
#include "Common/Swap.h"
#include "Core/MemMap.h"
#include "Core/ConfigValues.h"
#include "GPU/ge_constants.h"
#include "GPU/GPUState.h"

enum CheckAlphaResult {
	// These are intended to line up with TexCacheEntry::STATUS_ALPHA_UNKNOWN, etc.
	CHECKALPHA_FULL = 0,
	CHECKALPHA_ANY = 4,
};

// For both of these, pitch must be aligned to 16 bits (as is the case on a PSP).
void DoSwizzleTex16(const u32 *ysrcp, u8 *texptr, int bxc, int byc, u32 pitch);
void DoUnswizzleTex16(const u8 *texptr, u32 *ydestp, int bxc, int byc, u32 pitch);

u32 StableQuickTexHash(const void *checkp, u32 size);

CheckAlphaResult CheckAlphaRGBA8888Basic(const u32 *pixelData, int stride, int w, int h);
CheckAlphaResult CheckAlphaABGR4444Basic(const u32 *pixelData, int stride, int w, int h);
CheckAlphaResult CheckAlphaRGBA4444Basic(const u32 *pixelData, int stride, int w, int h);
CheckAlphaResult CheckAlphaABGR1555Basic(const u32 *pixelData, int stride, int w, int h);
CheckAlphaResult CheckAlphaRGBA5551Basic(const u32 *pixelData, int stride, int w, int h);

// All these DXT structs are in the reverse order, as compared to PC.
// On PC, alpha comes before color, and interpolants are before the tile data.

struct DXT1Block {
	u8 lines[4];
	u16_le color1;
	u16_le color2;
};

struct DXT3Block {
	DXT1Block color;
	u16_le alphaLines[4];
};

struct DXT5Block {
	DXT1Block color;
	u32_le alphadata2;
	u16_le alphadata1;
	u8 alpha1; u8 alpha2;
};

void DecodeDXT1Block(u32 *dst, const DXT1Block *src, int pitch, int height, u32 *alpha);
void DecodeDXT3Block(u32 *dst, const DXT3Block *src, int pitch, int height);
void DecodeDXT5Block(u32 *dst, const DXT5Block *src, int pitch, int height);

uint32_t GetDXT1Texel(const DXT1Block *src, int x, int y);
uint32_t GetDXT3Texel(const DXT3Block *src, int x, int y);
uint32_t GetDXT5Texel(const DXT5Block *src, int x, int y);

static const u8 textureBitsPerPixel[16] = {
	16,  //GE_TFMT_5650,
	16,  //GE_TFMT_5551,
	16,  //GE_TFMT_4444,
	32,  //GE_TFMT_8888,
	4,   //GE_TFMT_CLUT4,
	8,   //GE_TFMT_CLUT8,
	16,  //GE_TFMT_CLUT16,
	32,  //GE_TFMT_CLUT32,
	4,   //GE_TFMT_DXT1,
	8,   //GE_TFMT_DXT3,
	8,   //GE_TFMT_DXT5,
	0,   // INVALID,
	0,   // INVALID,
	0,   // INVALID,
	0,   // INVALID,
	0,   // INVALID,
};

u32 GetTextureBufw(int level, u32 texaddr, GETextureFormat format);

inline bool AlphaSumIsFull(u32 alphaSum, u32 fullAlphaMask) {
	return fullAlphaMask != 0 && (alphaSum & fullAlphaMask) == fullAlphaMask;
}

template <typename IndexT, typename ClutT>
inline void DeIndexTexture(/*WRITEONLY*/ ClutT *dest, const IndexT *indexed, int length, const ClutT *clut, u32 *outAlphaSum) {
	// Usually, there is no special offset, mask, or shift.
	const bool nakedIndex = gstate.isClutIndexSimple();

	ClutT alphaSum = (ClutT)(-1);

	if (nakedIndex) {
		if (sizeof(IndexT) == 1) {
			for (int i = 0; i < length; ++i) {
				ClutT color = clut[*indexed++];
				alphaSum &= color;
				*dest++ = color;
			}
		} else {
			for (int i = 0; i < length; ++i) {
				ClutT color = (*indexed++) & 0xFF;
				alphaSum &= color;
				*dest++ = color;
			}
		}
	} else {
		for (int i = 0; i < length; ++i) {
			ClutT color = clut[gstate.transformClutIndex(*indexed++)];
			alphaSum &= color;
			*dest++ = color;
		}
	}

	*outAlphaSum = alphaSum;
}

template <typename IndexT, typename ClutT>
inline void DeIndexTexture(/*WRITEONLY*/ ClutT *dest, const u32 texaddr, int length, const ClutT *clut, u32 *outAlphaSum) {
	const IndexT *indexed = (const IndexT *) Memory::GetPointer(texaddr);
	DeIndexTexture(dest, indexed, length, clut, outAlphaSum);
}

template <typename ClutT>
inline void DeIndexTexture4(/*WRITEONLY*/ ClutT *dest, const u8 *indexed, int length, const ClutT *clut, u32 *outAlphaSum) {
	// Usually, there is no special offset, mask, or shift.
	const bool nakedIndex = gstate.isClutIndexSimple();

	ClutT alphaSum = (ClutT)(-1);
	if (nakedIndex) {
		for (int i = 0; i < length; i += 2) {
			u8 index = *indexed++;
			ClutT color0 = clut[index & 0xf];
			ClutT color1 = clut[index >> 4];
			dest[i + 0] = color0;
			dest[i + 1] = color1;
			alphaSum &= color0 & color1;
		}
	} else {
		for (int i = 0; i < length; i += 2) {
			u8 index = *indexed++;
			ClutT color0 = clut[gstate.transformClutIndex((index >> 0) & 0xf)];
			ClutT color1 = clut[gstate.transformClutIndex((index >> 4) & 0xf)];
			dest[i + 0] = color0;
			dest[i + 1] = color1;
			alphaSum &= color0 & color1;
		}
	}

	*outAlphaSum &= (u32)alphaSum;
}

template <typename ClutT>
inline void DeIndexTexture4Optimal(ClutT *dest, const u8 *indexed, int length, ClutT color) {
	for (int i = 0; i < length; i += 2) {
		u8 index = *indexed++;
		dest[i + 0] = color | ((index >> 0) & 0xf);
		dest[i + 1] = color | ((index >> 4) & 0xf);
	}
}

template <>
inline void DeIndexTexture4Optimal<u16>(u16 *dest, const u8 *indexed, int length, u16 color) {
	const u16_le *indexed16 = (const u16_le *)indexed;
	const u32 color32 = (color << 16) | color;
	u32 *dest32 = (u32 *)dest;
	for (int i = 0; i < length / 2; i += 2) {
		u16 index = *indexed16++;
		dest32[i + 0] = color32 | ((index & 0x00f0) << 12) | ((index & 0x000f) >> 0);
		dest32[i + 1] = color32 | ((index & 0xf000) <<  4) | ((index & 0x0f00) >> 8);
	}
}

inline void DeIndexTexture4OptimalRev(u16 *dest, const u8 *indexed, int length, u16 color) {
	const u16_le *indexed16 = (const u16_le *)indexed;
	const u32 color32 = (color << 16) | color;
	u32 *dest32 = (u32 *)dest;
	for (int i = 0; i < length / 2; i += 2) {
		u16 index = *indexed16++;
		dest32[i + 0] = color32 | ((index & 0x00f0) << 24) | ((index & 0x000f) << 12);
		dest32[i + 1] = color32 | ((index & 0xf000) << 16) | ((index & 0x0f00) <<  4);
	}
}

template <typename ClutT>
inline void DeIndexTexture4(ClutT *dest, const u32 texaddr, int length, const ClutT *clut) {
	const u8 *indexed = (const u8 *) Memory::GetPointer(texaddr);
	DeIndexTexture4(dest, indexed, length, clut);
}

template <typename ClutT>
inline void DeIndexTexture4Optimal(ClutT *dest, const u32 texaddr, int length, ClutT color) {
	const u8 *indexed = (const u8 *) Memory::GetPointer(texaddr);
	DeIndexTexture4Optimal(dest, indexed, length, color);
}
