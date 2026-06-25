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

#ifndef inc_ui_progress_bar_hh
#define inc_ui_progress_bar_hh

#include <ui/base.hh>

#include <functional>
#include <string>

#include "settings.hh"


namespace ui
{
	static inline ui::Screen progloc()
	{
		return ISET_PROGBAR_TOP ? ui::Screen::top : ui::Screen::bottom;
	}

	std::string loadingbar_serialize(u64, u64);
	std::string loadingbar_postfix(u64);
	std::string up_to_mib_serialize(u64, u64);
	std::string up_to_mib_postfix(u64);

	template <typename T, size_t S>
	class circular_buffer
	{
	public:
		circular_buffer()
			: ptr(0), len(0) { }

		void push(const T& el)
		{
			this->array[this->ptr++] = el;
			if(this->ptr == S)
				this->ptr = 0;
			if(this->len < S)
				++this->len;
		}

		T avg() const
		{
			/* XXX: Should this be += diffs[i] / diffs.size() to prevent overflow
			 *      or is that not accurate enough? */
			T ret = 0;
			for(size_t i = 0; i < this->len; ++i)
				ret += this->array[i];
			return ret / this->len;
		}

		/* UB for this->size() = 0 */
		void maxmin(T& max, T& min) const
		{
			max = this->array[0];
			min = this->array[0];
			for(size_t i = 1; i < this->len; ++i)
			{
				if(this->array[i] > max) max = this->array[i];
				if(this->array[i] < min) min = this->array[i];
			}
		}

		T& operator [] (size_t i)
		{ return this->array[i]; }
		T at(size_t i) const { return this->array[i]; }

		static constexpr size_t max_size() { return S; }
		size_t size() const { return this->len; }

		size_t start() const { return this->len == S ? (this->ptr == (S - 1) ? 0 : this->ptr + 1) : 0; }
		size_t next(size_t prev) const { return prev == (S - 1) ? 0 : prev + 1; }
		size_t end() const { return this->ptr; }

	private:
		T array[S];
		size_t ptr, len;

	};
	/* A shorter window reacts faster to changing Wi-Fi conditions while still
	 * smoothing the half-second-ish samples produced by 1 MiB chunks. */
	using SpeedBuffer = circular_buffer<float, 8>;

	UI_SLOTS_PROTO_EXTERN(ProgressBar_color)
	class ProgressBar : public ui::BaseWidget
	{ UI_WIDGET("ProgressBar")
	public:
		void setup(u64 part, u64 total);
		void setup(u64 total);
		void setup() override;

		void destroy() override;

		bool render(ui::Keys& keys) override;
		float height() override;
		float width() override;

		void update(u64 part, u64 total);
		void update(u64 part);

		void set_serialize(std::function<std::string(u64, u64)> cb);
		void set_postfix(std::function<std::string(u64)> cb);

		void activate();
		
		UI_BUILDER_EXTENSIONS()
		{
			UI_USING_BUILDER()
		public:
			ReturnValue use_speed()
			{
				this->instance().flags |= FLAG_SHOW_SPEED;
				this->instance().serialize = up_to_mib_serialize;
				this->instance().postfix = up_to_mib_postfix;
				this->instance().update_state();
				return this->return_value();
			}
		};

		const SpeedBuffer& speed_buffer() { return this->speedDiffs; }


	private:
		UI_SLOTS_PROTO(ProgressBar_color, 3)

		enum flag {
			FLAG_ACTIVE     = 0x1,
			FLAG_SHOW_SPEED = 0x2,
		};

		void update_state();

		u32 flags;
		float bcx, ex, w, targetw, outerw;
		u64 part, total;

		C2D_Text bc, a, d, e;
		C2D_TextBuf buf;

		std::function<std::string(u64, u64)> serialize = loadingbar_serialize;
		std::function<std::string(u64)> postfix = loadingbar_postfix;

		/* data for ETA/speed */
		u64 speed_window_start = 0;
		u64 speed_window_part = 0;
		float displayed_bytes_s = 0.0f;
		u64 display_part = 0;
		u64 display_update_time = 0;
		u64 display_total = 0;
		bool display_initialized = false;

		SpeedBuffer speedDiffs;

	};

	UI_SLOTS_PROTO_EXTERN(LatencyGraph_color)
	class LatencyGraph : public ui::BaseWidget
	{ UI_WIDGET("LatencyGraph")
	public:
		void setup(const SpeedBuffer& buffer);

		bool render(ui::Keys& keys) override;
		void set_x(float x) override;
		float height() override;
		float width() override;


	private:
		UI_SLOTS_PROTO(LatencyGraph_color, 1)

		const SpeedBuffer *buffer;
		float step, w;


	};
}

#endif
