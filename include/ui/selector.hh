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

#ifndef inc_ui_selector_hh
#define inc_ui_selector_hh

#include <ui/base.hh>

#include <functional>
#include <string>
#include <vector>


namespace ui
{
	namespace constants
	{
		constexpr float SEL_LABEL_DEFAULT_HEIGHT = 30.0f;
		constexpr float SEL_DEFAULT_FONTSIZ = 0.65f;
	}

	template <typename TEnum>
	class Selector : public ui::BaseWidget
	{ UI_WIDGET("Selector")
	public:
		void setup(const std::vector<std::string>& labels, const std::vector<TEnum>& values, TEnum *res = nullptr)
		{
			this->values = &values;
			this->res = res;

			this->set_labels(labels);

			using namespace constants;
			/* this is really just a hack from back when this widget was in exclusive mode only */
			this->resize(ui::screen_width(this->screen) - 50.0f, SEL_LABEL_DEFAULT_HEIGHT);
			this->x = ui::transform(this, ui::layout::center_x);
			this->y = ui::transform(this, ui::layout::center_y);
			this->assign_txty();

			static ui::slot_color_getter getters[] = {
				color_button, color_text, color_border
			};
			this->slots = ui::ThemeManager::global()->get_slots(this, "Selector", 3, getters);
		}

		void set_labels(const std::vector<std::string>& labels)
		{
			if (this->values->size() != labels.size())
				panic("attempting to set different amount of labels than there are loaded values");

			if (this->labels.size())
			{
				C2D_TextBufDelete(this->buf);
				this->labels.clear();
			}

			this->buf = C2D_TextBufNew(this->accumul_size(labels));

			for(const std::string& label : labels)
			{
				C2D_Text text;
				ui::parse_text(&text, this->buf, label.c_str());
				C2D_TextOptimize(&text);
				this->labels.push_back(text);
			}
		}

		void destroy() override
		{
			C2D_TextBufDelete(this->buf);
		}

		void set_x(float x) override
		{
			this->x = ui::transform(this, x);
			this->assign_txty();
		}

		void set_y(float y) override
		{
			this->y = ui::transform(this, y);
			this->assign_txty();
		}

		void autowrap() /* override */
		{
			using namespace constants;
			float xlarge = 0.0f, ylarge = 0.0f, x, y;
			for(C2D_Text& text : this->labels)
			{
				C2D_TextGetDimensions(&text, this->fontw, this->fonth, &x, &y);
				if(x > xlarge) xlarge = x;
				if(y > ylarge) ylarge = y;
			}
			/* we need to divide by 0.76 since resize will multiply by it */
			this->resize((xlarge + 6.0f) / 0.76f, ylarge + 2.0f);
			this->assign_txty();
		}

		void resize(float w, float h) /* override */
		{
			/* this is actually a little complicated */
			this->bw = w * 0.76f; /* the box gets 76% */
			this->triwp = w * 0.02f; /* the padding gets 2% (2x) */
			this->triw = w * 0.10f; /* the triangle gets 10% (2x) */
			this->h = h;
			this->assign_txty();
		}

		float width() override
		{
			return (this->triwp + this->triw) * 2 + this->bw;
		}

		float height() override
		{
			return this->h;
		}

		bool render(ui::Keys& keys) override
		{
			using namespace constants;

			// The following maths is very long and cursed
			// So it is recommended to do something else

			const float left_x = this->x;
			const float label_x = this->x + this->triw + this->triwp;
			const float right_x = label_x + this->bw + this->triwp;

			/* Three floating glass regions, separated only by space. */
			C2D_DrawRectSolid(label_x, this->y, this->z, this->bw, this->h, C2D_Color32(0,0,0,178));
			C2D_DrawRectSolid(label_x + 6.0f, this->y + 1.0f, this->z + 0.01f,
				this->bw - 12.0f, 1.0f, C2D_Color32(255,255,255,22));
			C2D_DrawRectSolid(left_x, this->y, this->z, this->triw, this->h, C2D_Color32(0,0,0,178));
			C2D_DrawRectSolid(right_x, this->y, this->z, this->triw, this->h, C2D_Color32(0,0,0,178));
			/* minus / plus glyphs */
			C2D_DrawRectSolid(left_x + 7.0f, this->y + this->h / 2.0f, this->z + 0.02f,
				this->triw - 14.0f, 1.0f, this->slots.get(2));
			C2D_DrawRectSolid(right_x + 7.0f, this->y + this->h / 2.0f, this->z + 0.02f,
				this->triw - 14.0f, 1.0f, this->slots.get(2));
			C2D_DrawRectSolid(right_x + this->triw / 2.0f, this->y + 7.0f, this->z + 0.02f,
				1.0f, this->h - 14.0f, this->slots.get(2));

			u32 kdown = this->exclusiveMode ? keys.kDown : 0;
			if(ui::is_touched(keys) && keys.touch.px >= left_x
				&& keys.touch.px <= left_x + this->triw
				&& keys.touch.py >= this->y && keys.touch.py <= this->y + this->h)
			{
				ui::set_touch_lock(keys);
				kdown |= KEY_LEFT;
			}
			if(ui::is_touched(keys) && keys.touch.px >= right_x
				&& keys.touch.px <= right_x + this->triw
				&& keys.touch.py >= this->y && keys.touch.py <= this->y + this->h)
			{
				ui::set_touch_lock(keys);
				kdown |= KEY_RIGHT;
			}

			// Draw label...
			C2D_DrawText(&this->labels[this->idx], C2D_WithColor, this->tx, this->ty, this->z, this->fontw, this->fonth, this->slots.get(1));

			// Take input...
			if(kdown & KEY_A)
			{
				if(this->res != nullptr)
					*this->res = (*this->values)[this->idx];
				return false;
			}

			if(kdown & (KEY_LEFT | KEY_L))
				this->wrap_minus();

			if(kdown & (KEY_RIGHT | KEY_R))
				this->wrap_plus();

			return !this->exclusiveMode || !(kdown & KEY_B);
		}

		void resize_children(float w, float h) /* override */
		{
			this->fontw = w;
			this->fonth = h;
		}

		void search_set_idx(TEnum value)
		{
			for(size_t i = 0; i < this->values->size(); ++i)
			{
				if((*this->values)[i] == value)
				{
					if(this->res != nullptr)
						*this->res = value;
					this->idx = i;
					this->assign_txty();
					break;
				}
			}
		}

		UI_BUILDER_EXTENSIONS()
		{
			UI_USING_BUILDER()
		public:
			ReturnValue exclusive_mode(bool b = true)
			{
				this->instance().exclusiveMode = b;
				return this->return_value();
			}

			ReturnValue seek_to(TEnum val)
			{
				this->instance().search_set_idx(val);
				return this->return_value();
			}

			ReturnValue when_changed(std::function<void(TEnum)> cb)
			{
				this->instance().change_callback = cb;
				return this->return_value();
			}
		};

	private:
		ui::SlotManager slots { nullptr };

		const std::vector<TEnum> *values;
		std::vector<C2D_Text> labels;
		bool exclusiveMode = true;
		TEnum *res = nullptr;
		std::function<void(TEnum)> change_callback = [](TEnum) -> void { };
		C2D_TextBuf buf;
		size_t idx = 0;

		float fontw = ui::constants::SEL_DEFAULT_FONTSIZ, fonth = ui::constants::SEL_DEFAULT_FONTSIZ;
		float triwp, triw, bw, h;
		float tx, ty;

		void assign_txty()
		{
			using namespace constants;
			float h, w;
			C2D_TextGetDimensions(&this->labels[this->idx], this->fontw, this->fonth, &w, &h);
			this->tx = this->x + this->triwp + this->triw + ((this->bw / 2.0f) - (w / 2.0f));
			this->ty = this->y + (this->h / 2) - (h / 2);
		}

		size_t accumul_size(const std::vector<std::string>& labels)
		{
			size_t ret = 0;
			for(const std::string& label : labels)
			{ ret += label.size() + 1; }
			return ret;
		}

		void wrap_minus()
		{
			if(this->idx > 0) --this->idx;
			else this->idx = this->labels.size() - 1;
			this->assign_txty();
			this->change_callback((*this->values)[this->idx]);
		}

		void wrap_plus()
		{
			if(this->idx < this->labels.size() - 1) ++this->idx;
			else this->idx = 0;
			this->assign_txty();
			this->change_callback((*this->values)[this->idx]);
		}

		UI_CTHEME_GETTER(color_button, ui::theme::button_background_color)
		UI_CTHEME_GETTER(color_text, ui::theme::text_color)
		UI_CTHEME_GETTER(color_border, ui::theme::button_border_color)

	};
}

#endif
