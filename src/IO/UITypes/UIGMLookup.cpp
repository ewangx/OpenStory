#include "UIGMLookup.h"

#include "../Window.h"
#include "../../Configuration.h"
#include "../../Constants.h"
#include "../../Data/ItemData.h"
#include "../../Net/Packets/MessagingPackets.h"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <cstdlib>
#include <cstdint>
#include <nlnx/nx.hpp>

namespace ms
{
	namespace
	{
		std::string lowercase(std::string value)
		{
			std::transform(value.begin(), value.end(), value.begin(),
				[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return value;
		}

		void add_entries(std::vector<UIGMLookup::Entry>& entries, nl::node parent, bool map)
		{
			for (nl::node node : parent)
			{
				const std::string key = node.name();
				if (key.empty() || !std::all_of(key.begin(), key.end(),
					[](unsigned char c) { return std::isdigit(c); }))
					continue;

				char* end = nullptr;
				long id = std::strtol(key.c_str(), &end, 10);
				if (*end || id <= 0 || id > INT32_MAX)
					continue;

				nl::node name_node = node[map ? "mapName" : "name"];
				if (!name_node)
					continue;

				std::string name = name_node;
				if (map && node["streetName"])
					name = (std::string)node["streetName"] + " : " + name;
				entries.push_back({ static_cast<int32_t>(id), name, lowercase(name) });
			}
		}
	}

	const std::vector<UIGMLookup::Entry>& UIGMLookup::maps()
	{
		static const std::vector<Entry> index = []
		{
			std::vector<Entry> entries;
			for (nl::node category : nl::nx::string["Map.img"])
				add_entries(entries, category, true);
			std::sort(entries.begin(), entries.end(),
				[](const Entry& a, const Entry& b) { return a.id < b.id; });
			return entries;
		}();
		return index;
	}

	const std::vector<UIGMLookup::Entry>& UIGMLookup::items()
	{
		static const std::vector<Entry> index = []
		{
			std::vector<Entry> entries;
			nl::node strings = nl::nx::string;
			for (const char* category : { "Consume.img", "Ins.img", "Cash.img", "Pet.img" })
				add_entries(entries, strings[category], false);
			add_entries(entries, strings["Etc.img"]["Etc"], false);
			for (nl::node category : strings["Eqp.img"]["Eqp"])
				add_entries(entries, category, false);
			std::sort(entries.begin(), entries.end(),
				[](const Entry& a, const Entry& b) { return a.id < b.id; });
			entries.erase(std::unique(entries.begin(), entries.end(),
				[](const Entry& a, const Entry& b) { return a.id == b.id; }), entries.end());
			return entries;
		}();
		return index;
	}

	UIGMLookup::UIGMLookup() : UIElement(Point<int16_t>(190, 110), Point<int16_t>(WIDTH, HEIGHT)),
		background(WIDTH, HEIGHT, Color::Name::BLACK, 0.9f),
		input_background(WIDTH - 24, 23, Color::Name::WHITE, 1.0f),
		action_background(58, 18, Color::Name::MEDIUMBLUE, 0.85f),
		quantity_background(62, 21, Color::Name::WHITE, 1.0f),
		quantity_button(22, 21, Color::Name::MEDIUMBLUE, 0.85f),
		query(Text::A12M, Text::LEFT, Color::Name::BLACK,
			Rectangle<int16_t>(Point<int16_t>(17, 62), Point<int16_t>(WIDTH - 17, 82)), 80),
		quantity_field(Text::A11M, Text::LEFT, Color::Name::BLACK,
			Rectangle<int16_t>(Point<int16_t>(94, 310), Point<int16_t>(149, 329)), 5),
		label(Text::A11M, Text::LEFT, Color::Name::WHITE),
		action_label(Text::A11M, Text::CENTER, Color::Name::WHITE)
	{
		update_screen(Constants::Constants::get().get_viewwidth(), Constants::Constants::get().get_viewheight());
		query.set_key_callback(KeyAction::Id::ESCAPE, [this]() { deactivate(); });
		quantity_field.set_key_callback(KeyAction::Id::ESCAPE, [this]() { deactivate(); });
		quantity_field.change_text("1");
		query.set_state(Textfield::State::FOCUSED);
	}

	UIGMLookup::~UIGMLookup()
	{
		release_focus();
	}

	void UIGMLookup::release_focus()
	{
		if (query.get_state() == Textfield::State::FOCUSED)
			query.set_state(Textfield::State::DISABLED);
		if (quantity_field.get_state() == Textfield::State::FOCUSED)
			quantity_field.set_state(Textfield::State::DISABLED);
	}

	void UIGMLookup::deactivate()
	{
		release_focus();
		UIElement::deactivate();
	}

	void UIGMLookup::toggle_active()
	{
		if (is_active())
			deactivate();
		else
		{
			makeactive();
			query.set_state(Textfield::State::NORMAL);
			quantity_field.set_state(Textfield::State::NORMAL);
			query.set_state(Textfield::State::FOCUSED);
		}
	}

	void UIGMLookup::update_screen(int16_t width, int16_t height)
	{
		position = Point<int16_t>(std::max<int16_t>(0, (width - WIDTH) / 2),
			std::max<int16_t>(0, (height - HEIGHT) / 2));
	}

	void UIGMLookup::draw(float) const
	{
		background.draw(position);
		label.change_text("GM ID Lookup");
		label.draw(position + Point<int16_t>(12, 9));
		label.change_text("X");
		label.draw(position + Point<int16_t>(WIDTH - 24, 9));
		label.change_text(show_maps ? "[Maps]    Items" : "Maps    [Items]");
		label.draw(position + Point<int16_t>(12, 35));
		input_background.draw(position + Point<int16_t>(12, 60));
		query.draw(position);
		if (query.empty() && query.get_state() != Textfield::State::FOCUSED)
		{
			label.change_text("Search name or ID...");
			label.draw(position + Point<int16_t>(19, 64));
		}
		if (!last_query.empty() && results.empty())
		{
			label.change_text("No matching IDs in local NX data.");
			label.draw(position + Point<int16_t>(17, 94));
		}
		for (int16_t row = 0; row < visible_rows() && offset + row < static_cast<int32_t>(results.size()); ++row)
		{
			const Entry& entry = *results[offset + row];
			int16_t y = 91 + row * row_height();
			if (!show_maps)
			{
				const Texture& icon = ItemData::get(entry.id).get_icon(false);
				if (icon.is_valid())
					icon.draw(position + Point<int16_t>(17, y + 2) + icon.get_origin());
			}
			std::string text = std::to_string(entry.id) + "  " + entry.name;
			label.change_text(text);
			while (label.width() > (show_maps ? 310 : 272) && text.size() > 4)
			{
				text.pop_back();
				while (!text.empty() && (static_cast<unsigned char>(text.back()) & 0xC0) == 0x80)
					text.pop_back();
				label.change_text(text + "...");
			}
			label.draw(position + Point<int16_t>(show_maps ? 17 : 56, y + (show_maps ? 0 : 11)));
			int16_t button_y = y + (show_maps ? 0 : 8);
			action_background.draw(position + Point<int16_t>(WIDTH - 72, button_y));
			action_label.change_text(show_maps ? "Warp" : "Drop");
			action_label.draw(position + Point<int16_t>(WIDTH - 43,
				button_y + (18 - action_label.height()) / 2 - 3));
		}
		if (!show_maps)
		{
			label.change_text("Quantity:");
			label.draw(position + Point<int16_t>(12, 313));
			quantity_background.draw(position + Point<int16_t>(91, 307));
			quantity_field.draw(position);
			quantity_button.draw(position + Point<int16_t>(158, 307));
			quantity_button.draw(position + Point<int16_t>(185, 307));
			label.change_text("-");
			label.draw(position + Point<int16_t>(166, 310));
			label.change_text("+");
			label.draw(position + Point<int16_t>(192, 310));
		}
		label.change_text(feedback.empty() ? "Click a result to copy ID. Scroll for more." : feedback);
		label.draw(position + Point<int16_t>(12, 337));
	}

	void UIGMLookup::update()
	{
		if (!Configuration::get().get_admin())
		{
			deactivate();
			return;
		}
		query.update(position);
		quantity_field.update(position);
		if (last_query != query.get_text())
			search();
	}

	int32_t UIGMLookup::selected_quantity() const
	{
		const std::string& text = quantity_field.get_text();
		int32_t value = 0;
		const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
		return parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size()
			&& value >= 1 && value <= 32767 ? value : 0;
	}

	void UIGMLookup::adjust_quantity(int32_t delta)
	{
		int32_t value = selected_quantity();
		quantity_field.change_text(std::to_string(std::clamp(value + delta, 1, 32767)));
		feedback.clear();
	}

	void UIGMLookup::use_entry(const Entry& entry)
	{
		if (!Configuration::get().get_admin())
			return;
		if (show_maps)
		{
			GeneralChatPacket("!warp " + std::to_string(entry.id), true).dispatch();
			feedback = "Requested warp to map " + std::to_string(entry.id);
			return;
		}

		const int32_t amount = selected_quantity();
		if (amount == 0)
		{
			feedback = "Enter a quantity from 1 to 32767.";
			return;
		}
		if (amount != 1 && (entry.id / 1000000 == 1 || entry.id / 10000 == 500))
		{
			feedback = "Equipment and pets can only drop one at a time.";
			return;
		}
		GeneralChatPacket("!drop " + std::to_string(entry.id) + " " + std::to_string(amount), true).dispatch();
		feedback = entry.id / 10000 == 500
			? "Requested pet drop (1-day expiration)."
			: "Requested drop of " + std::to_string(amount) + " x " + std::to_string(entry.id);
	}

	void UIGMLookup::search()
	{
		last_query = query.get_text();
		results.clear();
		offset = 0;
		feedback.clear();
		if (last_query.empty())
			return;
		const std::string needle = lowercase(last_query);
		for (const Entry& entry : show_maps ? maps() : items())
			if (entry.search.find(needle) != std::string::npos || std::to_string(entry.id).find(needle) != std::string::npos)
				results.push_back(&entry);
	}

	Cursor::State UIGMLookup::send_cursor(bool clicked, Point<int16_t> pos)
	{
		if (!Configuration::get().get_admin())
			return Cursor::State::IDLE;
		Point<int16_t> local = pos - position;
		if (clicked)
		{
			if (local.y() < 30 && local.x() > WIDTH - 40)
			{
				deactivate();
				return Cursor::State::CLICKING;
			}
			if (local.y() >= 32 && local.y() < 56 && local.x() < 140)
			{
				show_maps = local.x() < 72;
				if (quantity_field.get_state() == Textfield::State::FOCUSED)
					quantity_field.set_state(Textfield::State::NORMAL);
				query.set_state(Textfield::State::FOCUSED);
				search();
				return Cursor::State::CLICKING;
			}
			if (local.y() >= 90 && local.y() < 90 + visible_rows() * row_height())
			{
				int16_t row = (local.y() - 90) / row_height();
				int32_t index = offset + row;
				if (index < static_cast<int32_t>(results.size()))
				{
					int16_t button_y = 91 + row * row_height() + (show_maps ? 0 : 8);
					if (local.x() >= WIDTH - 72 && local.x() < WIDTH - 14
						&& local.y() >= button_y && local.y() < button_y + 18)
						use_entry(*results[index]);
					else
					{
						const std::string id = std::to_string(results[index]->id);
						Window::get().setclipboard(id);
						feedback = "Copied ID " + id;
					}
				}
				return Cursor::State::CLICKING;
			}
			if (!show_maps && local.y() >= 305 && local.y() < 331)
			{
				if (local.x() >= 158 && local.x() < 180)
					adjust_quantity(-1);
				else if (local.x() >= 185 && local.x() < 207)
					adjust_quantity(1);
				else
					return quantity_field.send_cursor(pos, clicked);
				return Cursor::State::CLICKING;
			}
		}
		if (!show_maps && quantity_field.send_cursor(pos, clicked) != Cursor::State::IDLE)
			return clicked ? Cursor::State::CLICKING : Cursor::State::CANCLICK;
		return query.send_cursor(pos, clicked);
	}

	void UIGMLookup::send_scroll(double yoffset)
	{
		if (yoffset == 0)
			return;
		offset = std::clamp(offset - (yoffset > 0 ? 3 : -3), 0,
			std::max(0, static_cast<int32_t>(results.size()) - visible_rows()));
	}

	void UIGMLookup::send_key(int32_t, bool pressed, bool escape)
	{
		if (pressed && escape)
			deactivate();
	}
}
