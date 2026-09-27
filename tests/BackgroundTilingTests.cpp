#include "Gameplay/MapleMap/BackgroundTiling.h"

#include <iostream>

int main()
{
	using ms::background_tile_counts;

	// Thieves' Hideout has a tiled background with no bitmap or tile size.
	auto empty = background_tile_counts(0, 0, 0, 0, 800, 600, true, true);
	if (empty.horizontal != 0 || empty.vertical != 0)
	{
		std::cerr << "Image-less backgrounds must schedule no tiles\n";
		return 1;
	}

	auto tiled = background_tile_counts(200, 100, 0, 0, 800, 600, true, true);
	if (tiled.horizontal != 7 || tiled.vertical != 9)
	{
		std::cerr << "Valid tiled backgrounds must use image dimensions\n";
		return 1;
	}

	auto horizontal = background_tile_counts(200, 100, 100, 100, 800, 600, true, false);
	if (horizontal.horizontal != 11 || horizontal.vertical != 1)
	{
		std::cerr << "One-axis tiling must leave the other axis unchanged\n";
		return 1;
	}

	return 0;
}
