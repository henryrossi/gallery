#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/stat.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

typedef u32 Fixed;
typedef s16 FWord;
typedef u64 longDateTime;

typedef struct {
        char scalar[5];
        u16 numTables;
        u16 searchRange;
        u16 entrySelector;
        u16 rangeShift;
} FontDirectory;

typedef struct {
        char tag[5];
        u32 checkSum;
        u32 offset;
        u32 length;
} TableDirectory;

typedef enum {
        UNICODE = 0,
        MACINTOSH = 1,
        RESERVED = 2,
        MICROSOFT = 3,
} TextPlatform;

typedef struct {
        TextPlatform platformId;
        u16 format;
        u16 length;
        u16 language;
        u16 segCountX2;
        u16 searchRange;
        u16 entrySelector;
        u16 rangeShift;
        u16 *endCode;         // len: segCount
        u16 *startCode;       // len: segCount
        u16 *idDelta;         // len: segCount
        u16 *idRangeOffset;   // len: segCount
        u16 *glyphIndexArray; // len: variable
} Cmap4;

typedef enum {
        ON_CURVE = (1 << 0),
        X_SHORT_VECTOR = (1 << 1),
        Y_SHORT_VECTOR = (1 << 2),
        REPEAT = (1 << 3),
        X_SAME_OR_POSITIVE = (1 << 4),
        Y_SAME_OR_POSITIVE = (1 << 5),
} GlyphOutlineFlags;

typedef struct {
        u16 numberOfContours;
        u16 xMin;
        u16 yMin;
        u16 xMax;
        u16 yMax;
        u16 *endPtsOfContours; // len: numberOfContours
        u16 instructionLength; //
        u8 *instructions;      // len: instructionLength
        u8 *flags;             // len: variable
        u8 *xCoordinates;      // len: variable (entries could be 1 or 2 bytes
        u8 *yCoordinates;      //      depending on corresponding flag
} GlyphData;

typedef struct {
        Fixed version;
        Fixed fontRevision;
        u32 checkSumAdjustment;
        u32 magicNumber;
        u16 flags;
        u16 unitsPerEm;
        longDateTime created;
        longDateTime modified;
        FWord xMin;
        FWord yMin;
        FWord xMax;
        FWord yMax;
        u16 macStyle;
        u16 lowestRecPPEM;
        s16 fontDirectionHint;
        s16 indexToLocFormat;
        s16 glyphDataFormat;
} Head;

typedef struct {
        Fixed version;
        u16 numGlyphs;
        u16 maxPoints;
        u16 maxContours;
        u16 maxComponentPoints;
        u16 maxComponentContours;
        u16 maxZones;
        u16 maxTwilightPoints;
        u16 maxStorage;
        u16 maxFunctionDefs;
        u16 maxInstructionsDefs;
        u16 maxStackElements;
        u16 maxSizeOfInstructions;
        u16 maxComponentElements;
        u16 maxComponentDepth;
} Maxp;

typedef struct {
        u16 *xPoints;
        u16 *yPoints;
        u8 *flags;
        u16 numberOfPoints;
} GlyphContours;

typedef struct {
        u16 numberOfContours;
        FWord xMin;
        FWord yMin;
        FWord xMax;
        FWord yMax;
        GlyphContours *contours;
} Glyph;

typedef struct {
        FontDirectory dir;
        TableDirectory *tables;
        Cmap4 cmap;
        GlyphData *glyphData;
        Head head;
        Maxp maxp;
        void *glyphLocationOffsets;
} TTFFont;

typedef struct {
        u8 *data;
        u8 *pos;
        u64 size;
} FontParser;

typedef struct {

} TTFParser;

static u8 ttf_read_U8(FontParser *p) { return *(p->pos++); }

static u16 ttf_read_U16(FontParser *p) {
        u16 v1 = *(p->pos++);
        u16 v2 = *(p->pos++);
        return (v1 << 8 | v2);
}

static u32 ttf_read_U32(FontParser *p) {
        u32 v1 = *(p->pos++);
        u32 v2 = *(p->pos++);
        u32 v3 = *(p->pos++);
        u32 v4 = *(p->pos++);
        return (v1 << 24 | v2 << 16 | v3 << 8 | v4);
}

static void ttf_read_name(FontParser *p, char *name) {
        name[0] = *(p->pos++);
        name[1] = *(p->pos++);
        name[2] = *(p->pos++);
        name[3] = *(p->pos++);
        name[4] = 0;
}

static void ttf_move_parser_pos(FontParser *p, u32 offset) {
        assert(offset < p->size && "Error - moving parser pos out of bounds");
        p->pos = p->data + offset;
}

static void ttf_parser_skip(FontParser *p, u32 offset) {
        assert((p->pos - p->data) + offset < p->size
               && "Error - skipping parser pos out of bounds");
        p->pos += offset;
}

static u32 ttf_find_table_by_tag(TTFFont *f, const char *tag) {
        for (u32 i = 0; i < f->dir.numTables; i++) {
                if (strncmp(tag, f->tables[i].tag, 4) == 0) {
                        return i;
                }
        }
        return UINT32_MAX;
}

static u16 ttf_find_glyhp_index(TTFFont *f, char c) {
        u16 index = 0xFFFF;
        for (u32 i = 0; f->cmap.endCode[i] != 0xFFFF; i++) {
                if (c <= f->cmap.endCode[i]) {
                        if (c >= f->cmap.startCode[i]) {
                                if (f->cmap.idRangeOffset[i] == 0) {
                                        return f->cmap.idDelta[i] + c;
                                } else {
                                        printf("Character code uses the glyph "
                                               "array\n");
                                        return 0xFFFF;
                                }
                        } else {
                                return 0xFFFF;
                        }
                }
        }

        return index;
}

static int ttf_read_file(const char *filename, FontParser *b) {
        struct stat s;
        stat(filename, &s);

        b->data = malloc(s.st_size);
        b->pos = b->data;
        b->size = s.st_size;

        FILE *fp = fopen(filename, "rb");
        if (!fp) {
                fprintf(stderr, "Failed to open file: %s\n", filename);
                return 0;
        }
        if (!fread(b->data, b->size, 1, fp)) {
                fprintf(stderr, "Failed to read file: %s\n", filename);
                fclose(fp);
                return 0;
        }
        fclose(fp);

        return 1;
}

void ttf_parse_cmap(TTFFont *font, FontParser *p) {
        u32 cmap = ttf_find_table_by_tag(font, "cmap");
        ttf_move_parser_pos(p, font->tables[cmap].offset);
        ttf_read_U16(p);
        u16 cmapNumSubtables = ttf_read_U16(p);
        // first subtable is unicode
        ttf_parser_skip(p, 4);
        u32 subtableOffset = ttf_read_U32(p);
        ttf_move_parser_pos(p, font->tables[cmap].offset + subtableOffset);
        // Note: spec states no need to support formats besides 4 or 12. Lato
        // uses 4.
        font->cmap.format = ttf_read_U16(p);
        font->cmap.length = ttf_read_U16(p);
        font->cmap.language = ttf_read_U16(p);
        font->cmap.segCountX2 = ttf_read_U16(p);
        font->cmap.searchRange = ttf_read_U16(p);
        font->cmap.entrySelector = ttf_read_U16(p);
        font->cmap.rangeShift = ttf_read_U16(p);
        font->cmap.endCode = malloc(font->cmap.segCountX2);
        u32 segCount = font->cmap.segCountX2 / 2;
        for (u32 i = 0; i < segCount; i++) {
                font->cmap.endCode[i] = ttf_read_U16(p);
        }
        ttf_parser_skip(p, 2);
        font->cmap.startCode = malloc(font->cmap.segCountX2);
        for (u32 i = 0; i < segCount; i++) {
                font->cmap.startCode[i] = ttf_read_U16(p);
        }
        font->cmap.idDelta = malloc(font->cmap.segCountX2);
        for (u32 i = 0; i < segCount; i++) {
                font->cmap.idDelta[i] = ttf_read_U16(p);
        }
        font->cmap.idRangeOffset = malloc(font->cmap.segCountX2);
        for (u32 i = 0; i < segCount; i++) {
                font->cmap.idRangeOffset[i] = ttf_read_U16(p);
        }
        u32 glyphIndexArraySize
            = font->cmap.length - (font->cmap.segCountX2 * 4 + 16);
        if (glyphIndexArraySize == 0) {
                font->cmap.glyphIndexArray = NULL;
        } else {
                font->cmap.glyphIndexArray = malloc(glyphIndexArraySize);
                for (u32 i = 0; i < glyphIndexArraySize / 2; i++) {
                        font->cmap.glyphIndexArray[i] = ttf_read_U16(p);
                }
        }
}

void ttf_parse_head(TTFFont *font, FontParser *p) {
        u32 head = ttf_find_table_by_tag(font, "head");
        ttf_move_parser_pos(p, font->tables[head].offset);
        font->head.version = ttf_read_U32(p);
        font->head.fontRevision = ttf_read_U32(p);
        font->head.checkSumAdjustment = ttf_read_U32(p);
        font->head.magicNumber = ttf_read_U32(p);
        font->head.flags = ttf_read_U16(p);
        font->head.unitsPerEm = ttf_read_U16(p);
        ttf_parser_skip(p, 16);
        // font->head.created = ttf_read_U64(p);
        // font->head.modified= ttf_read_U64(p);
        font->head.xMin = ttf_read_U16(p);
        font->head.yMin = ttf_read_U16(p);
        font->head.xMax = ttf_read_U16(p);
        font->head.yMax = ttf_read_U16(p);
        font->head.macStyle = ttf_read_U16(p);
        font->head.lowestRecPPEM = ttf_read_U16(p);
        font->head.fontDirectionHint = ttf_read_U16(p);
        font->head.indexToLocFormat = ttf_read_U16(p);
        font->head.glyphDataFormat = ttf_read_U16(p);
}

void ttf_parse_maxp(TTFFont *font, FontParser *p) {
        u32 maxp = ttf_find_table_by_tag(font, "maxp");
        ttf_move_parser_pos(p, font->tables[maxp].offset);
        font->maxp.version = ttf_read_U32(p);
        font->maxp.numGlyphs = ttf_read_U16(p);
        font->maxp.maxPoints = ttf_read_U16(p);
        font->maxp.maxContours = ttf_read_U16(p);
        font->maxp.maxComponentPoints = ttf_read_U16(p);
        font->maxp.maxComponentContours = ttf_read_U16(p);
        font->maxp.maxZones = ttf_read_U16(p);
        font->maxp.maxTwilightPoints = ttf_read_U16(p);
        font->maxp.maxStorage = ttf_read_U16(p);
        font->maxp.maxFunctionDefs = ttf_read_U16(p);
        font->maxp.maxInstructionsDefs = ttf_read_U16(p);
        font->maxp.maxStackElements = ttf_read_U16(p);
        font->maxp.maxSizeOfInstructions = ttf_read_U16(p);
        font->maxp.maxComponentElements = ttf_read_U16(p);
        font->maxp.maxComponentDepth = ttf_read_U16(p);
}

void ttf_parse_loca(TTFFont *font, FontParser *p) {
        u32 loca = ttf_find_table_by_tag(font, "loca");
        ttf_move_parser_pos(p, font->tables[loca].offset);
        u32 format = font->head.indexToLocFormat;
        u32 numGlyphs = font->maxp.numGlyphs;
        font->glyphLocationOffsets = malloc(numGlyphs * (format ? 4 : 2));
        if (format) {
                u32 *offsets = (u32 *)font->glyphLocationOffsets;
                for (u32 i = 0; i < numGlyphs; i++) {
                        offsets[i] = ttf_read_U32(p);
                }
        } else {
                u16 *offsets = (u16 *)font->glyphLocationOffsets;
                for (u32 i = 0; i < numGlyphs; i++) {
                        offsets[i] = ttf_read_U16(p);
                }
        }
}

u32 ttf_find_glyph_offset(TTFFont *font, u32 glyphIndex) {
        u32 format = font->head.indexToLocFormat;
        printf("format: %d\n", format);
        if (format) {
                u32 *offsets = (u32 *)font->glyphLocationOffsets;
                return offsets[glyphIndex];
        } else {
                u16 *offsets = (u16 *)font->glyphLocationOffsets;
                return offsets[glyphIndex] * 2;
        }
}

int main(int argc, char *argv[]) {
        FontParser p = { 0 };

        const char *fontFilename = "Lato-Regular.ttf";
        int res = ttf_read_file(fontFilename, &p);
        if (res == 0) {
                return 1;
        }

        TTFFont font = { 0 };
        ttf_read_name(&p, font.dir.scalar);
        font.dir.numTables = ttf_read_U16(&p);
        font.dir.searchRange = ttf_read_U16(&p);
        font.dir.entrySelector = ttf_read_U16(&p);
        font.dir.rangeShift = ttf_read_U16(&p);

        font.tables = malloc(sizeof(TableDirectory) * font.dir.numTables);
        for (u32 i = 0; i < font.dir.numTables; i++) {
                ttf_read_name(&p, font.tables[i].tag);
                font.tables[i].checkSum = ttf_read_U32(&p);
                font.tables[i].offset = ttf_read_U32(&p);
                font.tables[i].length = ttf_read_U32(&p);
        }

        ttf_parse_cmap(&font, &p);
        ttf_parse_head(&font, &p);
        ttf_parse_maxp(&font, &p);
        ttf_parse_loca(&font, &p);

        u16 bGlyphIndex = ttf_find_glyhp_index(&font, 'b');

        u32 glyphOffset = ttf_find_glyph_offset(&font, bGlyphIndex);
        printf("glyph offset %d\n", glyphOffset);

        u32 glyf = ttf_find_table_by_tag(&font, "glyf");
        ttf_move_parser_pos(&p, font.tables[glyf].offset);
        ttf_parser_skip(&p, glyphOffset);
        printf("number of contours in glyph b: %d\n", ttf_read_U16(&p));
        ttf_parser_skip(&p, 8);
        u16 glyphEndPoint = ttf_read_U16(&p);
        printf("end points of contour 1: %d\n", glyphEndPoint);
        u16 glyphEndPoint2 = ttf_read_U16(&p);
        printf("end points of contour 2: %d\n", glyphEndPoint2);
        u16 instructionLength = ttf_read_U16(&p);
        ttf_parser_skip(&p, instructionLength);
        u32 numFlags = 1;
        for (;; numFlags++) {
                u8 flag = ttf_read_U8(&p);
                printf("x is short: %d, y is short: %d\n",
                       (flag & (1 << 1)) ? 1 : 0, (flag & (1 << 2)) ? 1 : 0);
                if (flag & (1 << 3)) {
                        break;
                }
        }

        return 0;
}
