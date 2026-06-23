
#ifndef inc_ui_popup_hh
#define inc_ui_popup_hh

#include <ui/base.hh>


namespace ui
{
	class PopUp : public BaseWidget
	{ UI_WIDGET("PopUp")
		friend class PopUpContent;

	public:
		static constexpr float x_padding = 10.0f;
		static constexpr float y_padding = 10.0f;
		static constexpr float x_margin = 4.0f;
		static constexpr float y_margin = 4.0f;

		enum ClaimMode {
			DontClaimScreen,
			ClaimScreen,
		};

		void setup(float w, float h)
		{
			this->pinput = ui::is_exclusive_input();
			this->pobscure = ui::is_obscured();

			this->w = w;
			this->h = h;
			this->entrance = 0.0f;
			this->last_lift = 0.0f;
		}

		void destroy() override
		{
			ui::exclusive_input(this->pinput);
			ui::obscure(this->pobscure);
		}

		float height() override { return this->h; }
		float width() override { return this->w; }

		void finalize() override
		{
			/* We need to set offsets within the group */
			this->group.translate(this->x + x_margin, this->y + y_margin);
		}

		bool render(ui::Keys& keys) override
		{
			this->entrance += (1.0f - this->entrance) * 0.24f;
			float lift = (1.0f - this->entrance) * 9.0f;
			this->group.translate(0.0f, lift - this->last_lift);
			this->last_lift = lift;
			u8 panel_alpha = (u8)(196.0f * this->entrance);
			u8 edge_alpha = (u8)(26.0f * this->entrance);
			u8 accent_alpha = (u8)(255.0f * this->entrance);
			C2D_DrawRectSolid(this->x, this->y + lift, ui::layer::above_image,
				this->w, this->h, C2D_Color32(0,0,0,panel_alpha));
			C2D_DrawRectSolid(this->x + 8.0f, this->y + lift + 1.0f, ui::layer::top,
				this->w - 16.0f, 1.0f, C2D_Color32(255,255,255,edge_alpha));
			C2D_DrawRectSolid(this->x + this->w - 12.0f, this->y + lift + 8.0f, ui::layer::top,
				3.0f, 3.0f, C2D_Color32(255,164,204,accent_alpha));
			return this->group.render_all(keys);
		}

		void resize(float w, float h)
		{
			this->w = w;
			this->h = h;
			/* We need to reconfigure are children to set the new max width and height */
			for(ui::BaseWidget *wid : this->group.widgets())
			{
				wid->set_max_height(this->inner_height());
				wid->set_max_width(this->inner_width());
			}
		}

		template <typename Func>
		static void pop_up(ui::Screen screen, float w, float h, ClaimMode mode, Func configure_popup)
		{
			PopUp *me;
			ui::builder<PopUp>(screen, w, h)
				.z(ui::layer::above_image)
				.exclusive_input() /* if the screen is claimed this has to be true, otherwise it doesn't matter */
				.add_to(&me, ui::RenderQueue::global());

			configure_popup(me);

			/* We can only detect it here because the callback may change dimensions */
			me->set_x(ui::screen_width(screen) - me->width() - x_padding);
			me->set_y(ui::screen_height() - me->height() - y_padding);
			/* We need to recall finalize() because we changed this->group */
			me->finalize();

			if(mode == ClaimScreen)
			{
				ui::exclusive_input();
				ui::obscure();
			}
		}

		template <typename Func>
		static void pop_up(ui::Screen screen, ClaimMode mode, Func func)
		{
			PopUp::pop_up(screen, 0.0f, 0.0f, mode, func);
		}

		template <typename Func>
		static void pop_up(ui::Screen screen, Func func)
		{
			PopUp::pop_up(screen, DontClaimScreen, func);
		}

		/* Must be called outside a frame! */
		void close()
		{
			ui::RenderQueue::global()->remove(this);
		}

		template <typename T, typename ... Ts>
		ui::builder<T> make_builder(Ts&&... args)
		{
			return std::move(ui::builder<T>(this->screen, args...)
				.max_height(this->inner_height())
				.max_width(this->inner_width())
				.z(ui::layer::top));
		}

		void add(ui::BaseWidget *widget)
		{
			panic_assert(widget->renders_on() == this->screen || widget->renders_on() == ui::Screen::none,
				"tried to add widget not bound on same screen");
			this->group.add(widget);
		}

		ui::BaseWidget *back()
		{
			return this->group.back();
		}

		void fit_to_content()
		{
			if(this->group.size())
			{
				ui::BaseWidget *right = this->group.rightmost();
				ui::BaseWidget *low = this->group.lowest();
				/* What we have here is only the inner width, so we have to convert it */
				this->resize(right->get_x() + right->width() + 2 * x_margin, low->get_y() + low->height() + 2 * y_margin);
			}
		}

	protected:
		WidgetGroup group;
		float w, h;
		float entrance;
		float last_lift;
		bool pinput, pobscure;

		float inner_height() { return this->h > 2 * y_margin ? this->h - 2 * y_margin : this->h; }
		float inner_width() { return this->w > 2 * x_margin ? this->w - 2 * x_margin : this->w; }

	};
}

#endif
