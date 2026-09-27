#pragma once

#include "../UIElement.h"
#include "../Components/Textfield.h"
#include "../../Graphics/Geometry.h"
#include "../../Graphics/Text.h"

#include <vector>

namespace ms
{
	// Client-side name/ID lookup for GM commands. Open with /gmlookup.
	class UIGMLookup : public UIElement
	{
	public:
		static constexpr Type TYPE = UIElement::Type::GMLOOKUP;
		static constexpr bool FOCUSED = false;
		static constexpr bool TOGGLED = true;

		UIGMLookup();
		~UIGMLookup() override;
		void draw(float inter) const override;
		void update() override;
		void update_screen(int16_t width, int16_t height) override;
		void deactivate();
		void toggle_active() override;
		Cursor::State send_cursor(bool clicked, Point<int16_t> pos) override;
		void send_scroll(double yoffset) override;
		void send_key(int32_t keycode, bool pressed, bool escape) override;
		Type get_type() const override { return TYPE; }

		struct Entry { int32_t id; std::string name; std::string search; };

	private:
		static const std::vector<Entry>& maps();
		static const std::vector<Entry>& items();
		void search();
		int32_t selected_quantity() const;
		void adjust_quantity(int32_t delta);
		void use_entry(const Entry& entry);
		void release_focus();

		static constexpr int16_t WIDTH = 420;
		static constexpr int16_t HEIGHT = 358;
		int16_t visible_rows() const { return show_maps ? 10 : 5; }
		int16_t row_height() const { return show_maps ? 20 : 39; }
		ColorBox background;
		ColorBox input_background;
		ColorBox action_background;
		ColorBox quantity_background;
		ColorBox quantity_button;
		Textfield query;
		Textfield quantity_field;
		mutable Text label;
		mutable Text action_label;
		std::vector<const Entry*> results;
		std::string last_query;
		bool show_maps = true;
		int32_t offset = 0;
		std::string feedback;
	};
}
