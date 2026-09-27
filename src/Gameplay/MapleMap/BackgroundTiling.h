#pragma once

#include <cstdint>

namespace ms
{
	struct BackgroundTileCounts
	{
		int16_t horizontal;
		int16_t vertical;
	};

	inline BackgroundTileCounts background_tile_counts(int16_t image_width, int16_t image_height,
		int16_t tile_width, int16_t tile_height, int16_t view_width, int16_t view_height,
		bool repeat_x, bool repeat_y)
	{
		if (image_width <= 0 || image_height <= 0)
			return { 0, 0 };

		if (tile_width <= 0)
			tile_width = image_width;
		if (tile_height <= 0)
			tile_height = image_height;

		return {
			static_cast<int16_t>(repeat_x ? view_width / tile_width + 3 : 1),
			static_cast<int16_t>(repeat_y ? view_height / tile_height + 3 : 1)
		};
	}
}
