/* This file is part of 3hs
 * Copyright (C) 2021-2025 hShop developer team
 *
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include <3ds/font.h>
#include <c2d/font.h>
#include <stdlib.h>
#include <error.hh>

/* this font contains all supplemental characters, for more information see /supplement_font/README */
#define SUPPLEMENT_FONT_PATH "romfs:/supplemental_font.bcfnt"

C2D_Font ui__sysFontWithSupplement;


static void prepare_to_pass_to_fixPointers(CFNT_s *outFont, u32 offset)
{
#define FIX_PTR(ptr, type) (ptr = (type *) (((u32) ptr) - offset))
#define UNFIX_PTR(ptr, type) ((type *) (((u32) ptr) + ((u32) outFont)))
	FIX_PTR(outFont->finf.tglp, TGLP_s);
	FIX_PTR(UNFIX_PTR(outFont->finf.tglp, TGLP_s)->sheetData, u8);

	FIX_PTR(outFont->finf.cmap, CMAP_s);
	CMAP_s *cmap;
	for(cmap = UNFIX_PTR(outFont->finf.cmap, CMAP_s); cmap->next; cmap = UNFIX_PTR(cmap->next, CMAP_s))
		FIX_PTR(cmap->next, CMAP_s);

	CWDH_s *cwdh;
	FIX_PTR(outFont->finf.cwdh, CWDH_s);
	for(cwdh = UNFIX_PTR(outFont->finf.cwdh, CWDH_s); cwdh->next; cwdh = UNFIX_PTR(cwdh->next, CWDH_s))
		FIX_PTR(cwdh->next, CWDH_s);
#undef UNFIX_PTR
#undef FIX_PTR
}


Result font_merger_run(void)
{
	u8 *readOnlyFontData;
	Handle sharedFontHandle;

	long supplementSize, sysSize;
	FILE *fontFile;
	CFNT_s *outFont = NULL, *font = NULL;
	
	TGLP_s *sysTGLP = NULL;
	TGLP_s *supTGLP = NULL;

	int supTGLPDataSize = 0;
	int sysTGLPDataSize = 0;
	int startNewGlyphIdRange = 0;
	u8 *endTGLPDataSys = NULL;

	Result res;
	if(R_FAILED(res = APT_GetSharedFont(&sharedFontHandle, (u32 *) &readOnlyFontData)))
		return res;

	res = svcMapMemoryBlock(sharedFontHandle, 0, MEMPERM_READ, MEMPERM_DONTCARE);
	if(R_FAILED(res) && res != (Result) 0xE0A01BF5)
		goto exitA;

	/* the font is at offset 0x80 */
	font = (CFNT_s *) (readOnlyFontData + 0x80);
	sysSize = font->fileSize;

	fontFile = fopen(SUPPLEMENT_FONT_PATH, "r");
	if(!fontFile) { res = APPERR_OUT_OF_MEM; goto exitB; }

	fseek(fontFile, 0, SEEK_END);
	supplementSize = ftell(fontFile);
	fseek(fontFile, 0, SEEK_SET);

	/* TODO: Don't be so liberal in this allocation */
	outFont = (CFNT_s *)malloc(sysSize * 2 + supplementSize * 2);
	if(!outFont) { res = APPERR_OUT_OF_MEM; goto exitB; }

	memcpy(outFont, font, sysSize);
	/* leave an entire supplementSize of bytes free for our relinking purposes */
	font = (CFNT_s *) (((u8 *) outFont) + sysSize + sysSize + supplementSize);
	if(fread(font, 1, supplementSize, fontFile) != (size_t) supplementSize)
	{ res = APPERR_OUT_OF_MEM; goto exitB; }

	/* alright now for the strange part: merging the fonts */

	/* first we make both fonts readable */
	prepare_to_pass_to_fixPointers(outFont, (u32) readOnlyFontData + 0x80);
	fontFixPointers(outFont);
	fontFixPointers(font);

	/* now we do some assertions about the font; some parameters need to be the same in both fonts for a proper merge  */
#define assert_eq(a) if(font->a != outFont->a) { res = 0; goto exitC; }
	assert_eq(version)
	assert_eq(finf.tglp->sheetFmt)
	assert_eq(finf.tglp->sheetWidth)
	assert_eq(finf.tglp->sheetHeight)
	assert_eq(finf.tglp->nRows)
	assert_eq(finf.tglp->nLines)
	assert_eq(finf.tglp->cellWidth)
	assert_eq(finf.tglp->cellHeight)
	assert_eq(finf.tglp->baselinePos)
	assert_eq(finf.tglp->sheetSize)
#undef assert_eq

	if((u32) outFont->finf.tglp > (u32) outFont->finf.cwdh
		|| (u32) outFont->finf.tglp > (u32) outFont->finf.cmap)
	{ res = APPERR_INCOMPATIBLE_FONT; goto exitB; }

	sysTGLP = outFont->finf.tglp;
	supTGLP = font->finf.tglp;

	supTGLPDataSize = supTGLP->sheetSize * supTGLP->nSheets;
	sysTGLPDataSize = sysTGLP->sheetSize * sysTGLP->nSheets;
	endTGLPDataSys = sysTGLP->sheetData + sysTGLPDataSize;

	/* we need to allocate supTGLPDataSize extra for the TGLPData, lets move over after the end of sysTGLPData */
	memmove(endTGLPDataSys + supTGLPDataSize, endTGLPDataSys, (((u32) outFont) + sysSize) - (u32) endTGLPDataSys);

	/* now we copy supTGLPData after the end of sysTGLPData */
	memcpy(endTGLPDataSys, supTGLP->sheetData, supTGLPDataSize);

	/* relink CMAP and CWDH */
#define FIX_PTR(p) (((u32) (p)) + supTGLPDataSize)
	outFont->finf.cmap = (CMAP_s *) FIX_PTR(outFont->finf.cmap);
	CMAP_s *cmap;
	for(cmap = outFont->finf.cmap; cmap->next; cmap = cmap->next)
		cmap->next = (CMAP_s *) FIX_PTR(cmap->next);

	outFont->finf.cwdh = (CWDH_s *) FIX_PTR(outFont->finf.cwdh);
	CWDH_s *cwdh;
	for(cwdh = outFont->finf.cwdh; cwdh->next; cwdh = cwdh->next)
		cwdh->next = (CWDH_s *) FIX_PTR(cwdh->next);
#undef FIX_PTR

	/* now we gotta calc this range and then we modify nSheets */
	startNewGlyphIdRange = sysTGLP->nSheets * sysTGLP->nRows * sysTGLP->nLines;
	sysTGLP->nSheets += supTGLP->nSheets;

	/* and now we need to reallocate the CMAP and CWDH glyph ids */

	cmap = font->finf.cmap;
	/* our font has a single CMAP_TYPE_SCAN so this is fine */
	if(cmap->mappingMethod != CMAP_TYPE_SCAN || cmap->next)
	{ res = APPERR_INCOMPATIBLE_FONT; goto exitB; }

	for(u16 i = 0; i < cmap->nScanEntries; ++i)
		cmap->scanEntries[i].glyphIndex += startNewGlyphIdRange;

	cwdh = font->finf.cwdh;
	cwdh->startIndex += startNewGlyphIdRange;
	cwdh->endIndex += startNewGlyphIdRange;

	/* now we add the CMAP and CWDH of sup to the start of sys because the system font defines some
	 * codepoints to be 0xFFFF (= invalid) that we do actually have in our font */
	cmap->next = outFont->finf.cmap;
	outFont->finf.cmap = cmap;

	cwdh->next = outFont->finf.cwdh;
	outFont->finf.cwdh = cwdh;


	prepare_to_pass_to_fixPointers(outFont, (u32) outFont);
	ui__sysFontWithSupplement = C2D_FontLoadFromMem(outFont, supplementSize * 2 + sysSize * 2);
	if(!ui__sysFontWithSupplement) res = APPERR_OUT_OF_MEM;

exitB:
	fclose(fontFile);
exitA:
	svcCloseHandle(sharedFontHandle);
	free(outFont);
	return res;
exitC:
	prepare_to_pass_to_fixPointers(outFont, (u32) outFont);
	ui__sysFontWithSupplement = C2D_FontLoadFromMem(outFont, sysSize);
	goto exitB;
}

void font_merger_destroy(void)
{
	C2D_FontFree(ui__sysFontWithSupplement);
}

