#pragma once

#include "../Graphics/Animation.h"

#include <cstdint>
#include <string>
#include <vector>

namespace ms
{
	class Player;

	class DirectionScene
	{
	public:
		void load(const std::string& path, Player& player);
		void clear(Player& player);
		void update(Player& player);
		void draw(float alpha) const;
		int32_t take_pending_field();
		bool is_active() const;

	private:
		struct Visual
		{
			Visual(nl::node src, nl::node command);

			Animation animation;
			int32_t start;
			int32_t x;
			int32_t y;
			int32_t dx;
			int32_t dy;
			int32_t duration;
			int32_t z;
		};

		struct Event
		{
			enum class Type
			{
				FIELD,
				SOUND,
				LOOK,
				ACTION
			};

			Type type;
			int32_t start;
			int32_t field = -1;
			std::string sound;
			std::string action;
			std::vector<int32_t> equips;
			bool fired = false;
		};

		void add_sound_event(int32_t start, const std::string& path);
		void play_sound(const std::string& path) const;

		std::vector<Visual> visuals;
		std::vector<Event> events;
		int32_t elapsed = 0;
		int32_t previous_elapsed = 0;
		int32_t pending_field = -1;
		bool active = false;
	};
}
