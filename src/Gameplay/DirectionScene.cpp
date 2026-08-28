#include "DirectionScene.h"

#include "../Audio/Audio.h"
#include "../Character/Player.h"
#include "../Constants.h"

#ifdef USE_NX
#include <nlnx/nx.hpp>
#endif

#include <algorithm>
#include <cctype>

namespace ms
{
	static nl::node resolve_effect_path(const std::string& path)
	{
		const std::string prefix = "Effect/";
		return nl::nx::effect.resolve(path.compare(0, prefix.size(), prefix) == 0
			? path.substr(prefix.size())
			: path);
	}

	static nl::node resolve_animation_source(nl::node source)
	{
		nl::node nested = source["0"];

		if (nested && nested.data_type() != nl::node::type::bitmap
			&& nested["0"].data_type() == nl::node::type::bitmap)
			return nested;

		return source;
	}

	DirectionScene::Visual::Visual(nl::node src, nl::node command)
		: animation(resolve_animation_source(src)),
		  start(static_cast<int32_t>(command["start"].get_integer())),
		  x(static_cast<int32_t>(command["x"].get_integer())),
		  y(static_cast<int32_t>(command["y"].get_integer())),
		  dx(static_cast<int32_t>(command["x1"].get_integer())),
		  dy(static_cast<int32_t>(command["y1"].get_integer())),
		  duration(static_cast<int32_t>(command["duration"].get_integer())),
		  z(static_cast<int32_t>(command["z"].get_integer()))
	{
	}

	void DirectionScene::load(const std::string& path, Player& player)
	{
		clear(player);
		nl::node scene = resolve_effect_path(path);

		if (!scene)
			return;

		for (nl::node command : scene)
		{
			int32_t type = static_cast<int32_t>(command["type"].get_integer());
			int32_t start = static_cast<int32_t>(command["start"].get_integer());

			if (type == 0)
			{
				std::string visual_path = command["visual"].get_string();
				nl::node visual = resolve_effect_path(visual_path);

				if (visual)
					visuals.emplace_back(visual, command);

				std::string sound = command["sound"].get_string();

				if (!sound.empty())
					add_sound_event(start, sound);
			}
			else if (type == 2 && command["field"])
			{
				Event event;
				event.type = Event::Type::FIELD;
				event.start = start;
				event.field = static_cast<int32_t>(command["field"].get_integer());
				events.push_back(event);
			}
			else if (type == 3)
			{
				Event event;
				event.type = Event::Type::LOOK;
				event.start = start;

				for (nl::node entry : command)
				{
					std::string name = entry.name();

					if (!name.empty() && std::all_of(name.begin(), name.end(), [](unsigned char c) { return std::isdigit(c); }))
						event.equips.push_back(static_cast<int32_t>(entry.get_integer()));
				}

				events.push_back(event);
			}
			else if (type == 4)
			{
				Event event;
				event.type = Event::Type::ACTION;
				event.start = start;
				event.action = command["action"].get_string();
				events.push_back(event);
			}
			else if (type == 5)
			{
				add_sound_event(start, command["sound"].get_string());
			}
		}

		std::stable_sort(visuals.begin(), visuals.end(), [](const Visual& left, const Visual& right)
		{
			return left.z < right.z;
		});
		std::stable_sort(events.begin(), events.end(), [](const Event& left, const Event& right)
		{
			return left.start < right.start;
		});

		active = !visuals.empty() || !events.empty();
	}

	void DirectionScene::clear(Player& player)
	{
		player.end_direction_look();
		visuals.clear();
		events.clear();
		elapsed = 0;
		previous_elapsed = 0;
		pending_field = -1;
		active = false;
	}

	void DirectionScene::update(Player& player)
	{
		if (!active)
			return;

		previous_elapsed = elapsed;
		elapsed += Constants::TIMESTEP;

		for (Visual& visual : visuals)
		{
			if (elapsed >= visual.start)
			{
				int32_t active_time = elapsed - std::max(previous_elapsed, visual.start);

				if (active_time > 0)
					visual.animation.update(static_cast<uint16_t>(active_time));
			}
		}

		for (Event& event : events)
		{
			if (event.fired || elapsed < event.start)
				continue;

			event.fired = true;

			if (event.type == Event::Type::FIELD)
				pending_field = event.field;
			else if (event.type == Event::Type::SOUND)
				play_sound(event.sound);
			else if (event.type == Event::Type::LOOK)
				player.begin_direction_look(event.equips);
			else if (!event.action.empty())
				player.play_direction_action(event.action);
		}
	}

	void DirectionScene::draw(float alpha) const
	{
		if (!active)
			return;

		int32_t scene_time = previous_elapsed
			+ static_cast<int32_t>((elapsed - previous_elapsed) * alpha);
		constexpr int16_t center_x = 400;
		constexpr int16_t center_y = 300;

		for (const Visual& visual : visuals)
		{
			if (scene_time < visual.start)
				continue;

			float progress = visual.duration > 0
				? std::min(1.0f, static_cast<float>(scene_time - visual.start) / visual.duration)
				: 0.0f;
			int16_t x = static_cast<int16_t>(center_x + visual.x + visual.dx * progress);
			int16_t y = static_cast<int16_t>(center_y + visual.y + visual.dy * progress);
			visual.animation.draw(DrawArgument(Point<int16_t>(x, y)), alpha);
		}
	}

	int32_t DirectionScene::take_pending_field()
	{
		int32_t field = pending_field;
		pending_field = -1;
		return field;
	}

	bool DirectionScene::is_active() const
	{
		return active;
	}

	void DirectionScene::add_sound_event(int32_t start, const std::string& path)
	{
		if (path.empty())
			return;

		Event event;
		event.type = Event::Type::SOUND;
		event.start = start;
		event.sound = path;
		events.push_back(event);
	}

	void DirectionScene::play_sound(const std::string& path) const
	{
		const std::string sound_prefix = "Sound/";
		const std::string effect_prefix = "Effect/";
		nl::node source;

		if (path.compare(0, sound_prefix.size(), sound_prefix) == 0)
			source = nl::nx::sound.resolve(path.substr(sound_prefix.size()));
		else if (path.compare(0, effect_prefix.size(), effect_prefix) == 0)
			source = nl::nx::effect.resolve(path.substr(effect_prefix.size()));

		if (source)
			Sound(source).play();
	}
}
