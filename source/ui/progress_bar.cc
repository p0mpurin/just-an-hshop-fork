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

#include <ui/progress_bar.hh>

#include <algorithm>

#define OUTER_W(screen) (ui::screen_width(screen) - (X_OFFSET * 2))
#define GRAPH_STEP(x) ((ui::screen_width(screen) - ((x) * 2)) / (float) (ui::SpeedBuffer::max_size() - 2))
#define LATENCY_BAR_HEIGHT 50.0f
#define TEXT_DIM 0.65f
#define X_OFFSET 10
#define Y_OFFSET 30
#define Y_LEN 30


UI_CTHEME_GETTER(color_fg, ui::theme::progress_bar_foreground_color)
UI_CTHEME_GETTER(color_bg, ui::theme::progress_bar_background_color)
UI_CTHEME_GETTER(color_text, ui::theme::text_color)
UI_SLOTS(ui::ProgressBar_color, color_text, color_fg, color_bg)

UI_CTHEME_GETTER(color_graph, ui::theme::graph_line_color)
//static u32 color_graph() { return UI_COLOR(00,00,00,FF); }
UI_SLOTS(ui::LatencyGraph_color, color_graph)

std::string ui::loadingbar_serialize(u64 cur, u64 total)
{
	(void)total;
	return std::to_string(cur);
}

std::string ui::loadingbar_postfix(u64 n)
{
	return "";
}

std::string ui::up_to_mib_serialize(u64 n, u64 largest)
{
	if(largest < 1024) return std::to_string(n); /* < 1 KiB */
	if(largest < 1024 * 1024)  return ui::floating_prec<float>((float) n / 1024); /* < 1 MiB */
	else return ui::floating_prec<float>((float) n / 1024 / 1024);
}

std::string ui::up_to_mib_postfix(u64 n)
{
	if(n < 1024) return " B"; /* < 1 KiB */
	if(n < 1024 * 1024) return " KiB"; /* < 1 MiB */
	else return " MiB";
}

/* class ProgressBar */

void ui::ProgressBar::setup(u64 part, u64 total)
{
	this->outerw = OUTER_W(this->screen);
	this->w = this->targetw = 0.0f;
	this->buf = C2D_TextBufNew(100); /* probably big enough */
	this->update(part, total);
	this->x = X_OFFSET; /* set a good default */
}

void ui::ProgressBar::setup(u64 total)
{ this->setup(0, total); }

void ui::ProgressBar::setup()
{ this->setup(0, 0); }

void ui::ProgressBar::destroy()
{
	C2D_TextBufDelete(this->buf);
}

bool ui::ProgressBar::render(ui::Keys& keys)
{
	(void) keys;
	this->w += (this->targetw - this->w) * 0.24f;
	if(this->targetw - this->w < 0.08f && this->targetw - this->w > -0.08f)
		this->w = this->targetw;
	/* outer border — subtle pink outline defining the bar area */
	C2D_DrawRectSolid(this->x - 1, this->y - 1, this->z,
		this->outerw + 2, Y_LEN + 2, this->slots.get(1));
	/* background rect */
	C2D_DrawRectSolid(this->x, this->y, this->z + 0.01f, this->outerw, Y_LEN, this->slots.get(2));

	// Overlay actual process
	if(this->w != 0)
		C2D_DrawRectSolid(X_OFFSET + 1, this->y + 1, this->z + 0.02f, this->w, Y_LEN - 2, this->slots.get(1));
	else
		C2D_DrawRectSolid(X_OFFSET + 1, this->y + 1, this->z + 0.02f, 0, Y_LEN - 2, this->slots.get(1));

	if(this->flags & ui::ProgressBar::FLAG_ACTIVE)
	{
		C2D_DrawText(&this->a, C2D_WithColor, X_OFFSET, this->y - Y_LEN + 2,
			this->z, TEXT_DIM, TEXT_DIM, this->slots.get(0));
		C2D_DrawText(&this->bc, C2D_WithColor, this->bcx, this->y - Y_LEN + 2,
			this->z, TEXT_DIM, TEXT_DIM, this->slots.get(0));

		if(this->flags & ui::ProgressBar::FLAG_SHOW_SPEED)
		{
			C2D_DrawText(&this->d, C2D_WithColor, X_OFFSET, this->y + Y_LEN + 2,
				this->z, TEXT_DIM, TEXT_DIM, this->slots.get(0));
			C2D_DrawText(&this->e, C2D_WithColor, this->ex, this->y + Y_LEN + 2,
				this->z, TEXT_DIM, TEXT_DIM, this->slots.get(0));
		}
	}

	return true;
}

static std::string format_duration(time_t secs)
{
	struct tm *tm = gmtime(&secs);
	char ret[10];
	if(tm->tm_hour != 0)
		sprintf(ret, "%02i:%02i:%02i", tm->tm_hour, tm->tm_min, tm->tm_sec);
	else
		sprintf(ret, "%02i:%02i", tm->tm_min, tm->tm_sec);
	return ret;
}

void ui::ProgressBar::update_state()
{
	u64 now = osGetTime();
	if(!this->display_initialized || this->display_total != this->total || this->part < this->display_part)
	{
		this->display_part = this->part;
		this->display_update_time = now;
		this->display_total = this->total;
		this->display_initialized = true;
	}

	// (a)        (b/c)
	// 90%         9/10
	// [==============]
	// 1MiB/s  1:00 ETA
	// (d)          (e)

	C2D_TextBufClear(this->buf);

	if(this->flags & ui::ProgressBar::FLAG_SHOW_SPEED)
	{
		if(!this->speed_window_start || this->part < this->speed_window_part)
		{
			this->speed_window_start = now;
			this->speed_window_part = this->part;
		}

		/* Direct sockets arrive in short bursts. Sample only complete
		 * ~1 second byte windows so render/event timing cannot become a
		 * fake 1-6 MiB/s spike. Blend consecutive windows lightly. */
		u64 window_ms = now - this->speed_window_start;
		if(window_ms >= 900)
		{
			const float sample = ((float)(this->part - this->speed_window_part))
				/ (window_ms / 1000.0f);
			if(this->displayed_bytes_s == 0.0f)
				this->displayed_bytes_s = sample;
			else
				this->displayed_bytes_s = this->displayed_bytes_s * 0.65f + sample * 0.35f;
			this->speedDiffs.push(this->displayed_bytes_s);
			this->speed_window_start = now;
			this->speed_window_part = this->part;
		}
		const float bytes_s = this->displayed_bytes_s;
		const char *format;
		float speed_i;
		/* we can use MiB/s */
		if(bytes_s >= (1024.0f * 1024.0f))
		{
			speed_i = bytes_s / (1024.0f * 1024.0f);
			format = "MiB/s";
		}
		/* if we have less than 1MiB/s speed we fall back to KiB/s */
		else
		{
			speed_i = bytes_s / 1024.0f;
			format = "KiB/s";
		}
		std::string speed = "SPEED  " + floating_prec<float>(speed_i) + " " + std::string(format);

		/* if we have no speed yet, we cannot know the ETA, so we just omit it */
		std::string eta = bytes_s ? "ETA  " + format_duration((this->total - this->part) / bytes_s) : "ETA  --:--";

		ui::parse_text(&this->d, this->buf, speed.c_str());
		ui::parse_text(&this->e, this->buf, eta.c_str());
		C2D_TextOptimize(&this->d);
		C2D_TextOptimize(&this->e);

		C2D_TextGetDimensions(&this->e, TEXT_DIM, TEXT_DIM, &this->ex, nullptr);
		this->ex = ui::screen_width(this->screen) - X_OFFSET - this->ex;
	}

	if(this->part >= this->total)
		this->display_part = this->part;
	else if(this->display_part < this->part)
	{
		u64 elapsed_ms = now - this->display_update_time;
		float visual_bytes_s = this->displayed_bytes_s > 0.0f
			? this->displayed_bytes_s
			: 1024.0f * 1024.0f;
		u64 advance = (u64)(visual_bytes_s * (elapsed_ms / 1000.0f));
		if(advance < 32 * 1024)
			advance = std::min<u64>(this->part - this->display_part, 32 * 1024);
		this->display_part = std::min<u64>(this->part, this->display_part + advance);
	}
	this->display_update_time = now;

	float perc = this->total == 0 ? 0.0f : ((float) this->display_part / this->total);
	this->targetw = (ui::screen_width(this->screen) - (X_OFFSET * 2) - 4) * perc;

	// Parse strings
	std::string bc = this->serialize(this->display_part, this->total) + "/" + this->serialize(this->total, this->total) + this->postfix(this->total);
	std::string a = floating_prec<float>(perc * 100, 1) + "%";

	ui::parse_text(&this->bc, this->buf, bc.c_str());
	ui::parse_text(&this->a, this->buf, a.c_str());

	C2D_TextOptimize(&this->bc);
	C2D_TextOptimize(&this->a);

	// Pad to right
	C2D_TextGetDimensions(&this->bc, TEXT_DIM, TEXT_DIM, &this->bcx, nullptr);
	this->bcx = ui::screen_width(this->screen) - X_OFFSET - this->bcx;
}

void ui::ProgressBar::set_postfix(std::function<std::string(u64)> cb)
{ this->postfix = cb; this->flags &= ~ui::ProgressBar::FLAG_SHOW_SPEED; }

void ui::ProgressBar::set_serialize(std::function<std::string(u64, u64)> cb)
{ this->serialize = cb; this->flags &= ~ui::ProgressBar::FLAG_SHOW_SPEED; }

void ui::ProgressBar::activate()
{ this->flags |= ui::ProgressBar::FLAG_ACTIVE; }

void ui::ProgressBar::update(u64 part, u64 total)
{ this->part = part; this->total = total; this->update_state(); }

void ui::ProgressBar::update(u64 part)
{ this->part = part; this->update_state(); }

float ui::ProgressBar::height()
{ return Y_LEN; }

float ui::ProgressBar::width()
{ return this->outerw; }

/* LatencyGraph */

void ui::LatencyGraph::setup(const ui::SpeedBuffer& buffer)
{
	this->buffer = &buffer;
	this->w = OUTER_W(this->screen);
	this->step = GRAPH_STEP(0);
}

void ui::LatencyGraph::set_x(float x)
{
	this->x = x;
	this->step = GRAPH_STEP(x);
}

bool ui::LatencyGraph::render(ui::Keys& keys)
{
	(void) keys;
	if(!this->buffer->size())
		return true; /* no data */

	size_t i = this->buffer->start(), end = this->buffer->end();
	float max, min;
	this->buffer->maxmin(max, min);
	float mult = LATENCY_BAR_HEIGHT / (max - min), avg = this->buffer->avg();
	float x0 = this->x, y0 = this->y - (this->buffer->at(i) - avg) * mult, x1, y1;
	for(i = this->buffer->next(i); i != end; i = this->buffer->next(i))
	{
		x1 = x0 + this->step;
		y1 = this->y - (this->buffer->at(i) - avg) * mult;
		C2D_DrawLine(x0, y0, this->slots[0], x1, y1, this->slots[0], 2.0f, this->z);
		x0 = x1;
		y0 = y1;
	}

	return true;
}

float ui::LatencyGraph::width()
{
	return this->w;
}

float ui::LatencyGraph::height()
{
	return LATENCY_BAR_HEIGHT;
}
