#ifndef COLMATERIALS_H
#define COLMATERIALS_H

#include <stdint.h>

struct ColMaterialColor {
	const char *name;
	uint8 r, g, b;
};

struct ColRGB {
	uint8 r, g, b;
};

static const ColMaterialColor gColGroupColors[14] = {
	{ "Road",     0x30, 0x30, 0x30 },
	{ "Concrete", 0x90, 0x90, 0x90 },
	{ "Gravel",   0x64, 0x5E, 0x53 },
	{ "Grass",    0x92, 0xC0, 0x32 },
	{ "Dirt",     0x77, 0x5F, 0x40 },
	{ "Sand",     0xE7, 0xE1, 0x7E },
	{ "Glass",    0xA7, 0xE9, 0xFC },
	{ "Wood",     0x93, 0x69, 0x44 },
	{ "Metal",    0xBF, 0xC8, 0xD5 },
	{ "Rock",     0xAF, 0xAA, 0xA0 },
	{ "Bushes",   0x2E, 0xA5, 0x63 },
	{ "Water",    0x64, 0x93, 0xE1 },
	{ "Misc",     0xF1, 0xAB, 0x07 },
	{ "Vehicle",  0xFF, 0xD4, 0xFD },
};

static const uint8 gColMaterialToGroupSA[179] = {
	0, 0, 0, 0, 1, 1, 2, 1, 1, 3,
	3, 3, 3, 3, 3, 3, 3, 9, 4, 3,
	4, 4, 10, 4, 4, 4, 4, 5, 5, 5,
	5, 5, 5, 1, 9, 9, 9, 11, 11, 4,
	10, 7, 7, 7, 6, 6, 6, 12, 12, 8,
	8, 8, 8, 8, 8, 8, 8, 8, 8, 12,
	12, 12, 8, 8, 8, 12, 12, 12, 9, 7,
	12, 7, 7,
	5, 5, 5, 5, 5, 5, 3, 3, 3, 4,
	4, 2, 5, 4, 4, 1, 12, 12, 12, 12,
	12, 12, 5, 5, 5, 5, 4, 2, 12, 12,
	12, 12, 12, 12, 12, 12, 9, 4, 10, 10,
	10, 10, 3, 3, 3, 3, 3, 3, 3, 3,
	3, 3, 4, 4, 3, 4, 1, 4, 4, 4,
	5, 4, 4, 2, 1, 1, 1, 1, 1, 2,
	4, 4, 5, 1, 4, 3, 3, 3, 3, 3,
	3, 3, 3, 9, 4, 4, 4, 5,
	12, 12, 3, 9, 8, 12, 8, 1, 12, 8,
	8, 12, 12, 8, 7, 7, 7, 6, 12, 12, 12
};

inline ColRGB
GetColMaterialColor(uint8 material)
{
	if(material < 179){
		uint8 group = gColMaterialToGroupSA[material];
		const ColMaterialColor &col = gColGroupColors[group];
		return { col.r, col.g, col.b };
	}
	return { 128, 128, 128 };
}

#endif