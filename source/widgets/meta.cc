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

#include "widgets/meta.hh"

#define META_PANEL() do { \
	C2D_DrawRectSolid(8.0f, 10.0f, -0.33f, 304.0f, 194.0f, C2D_Color32(0, 0, 0, 212)); \
	C2D_DrawRectSolid(8.0f, 10.0f, -0.30f, 304.0f, 1.0f, C2D_Color32(255, 255, 255, 34)); \
	C2D_DrawRectSolid(8.0f, 203.0f, -0.30f, 304.0f, 1.0f, C2D_Color32(255, 255, 255, 18)); \
	C2D_DrawRectSolid(18.0f, 72.0f, -0.28f, 282.0f, 1.0f, C2D_Color32(255, 255, 255, 20)); \
	C2D_DrawRectSolid(18.0f, 119.0f, -0.28f, 282.0f, 1.0f, C2D_Color32(255, 255, 255, 14)); \
	} while(0)

#define PAIR_I18N(val, title, val_i18n) do { \
	ui::builder<ui::Text>(this->screen, title) \
		.size(0.40f) \
		.x(this->get_x()) \
		.under(this->queue.back(), 3.0f) \
		.add_to(this->queue); \
	ui::builder<ui::Text>(this->screen, val) \
		.manual_i18n_update(val_i18n) \
		.size(0.58f) \
		.x(this->get_x()) \
		.under(this->queue.back(), -1.0f) \
		.scroll() \
		.add_to(this->queue); \
	} while(0)

#define PAIR(val, title) do { \
	ui::builder<ui::Text>(this->screen, title) \
		.size(0.40f) \
		.x(this->get_x()) \
		.under(this->queue.back(), 3.0f) \
		.add_to(this->queue); \
	ui::builder<ui::Text>(this->screen, val) \
		.size(0.58f) \
		.x(this->get_x()) \
		.under(this->queue.back(), -1.0f) \
		.scroll() \
		.add_to(this->queue); \
	} while(0)


/* CatMeta */

static void clip_q(ui::I18NEnabledRenderQueue &q, float base, float size = 0.65f)
{
	float totalHeight = base;
	for(ui::BaseWidget *wid : q.bot)
		totalHeight += wid->height();
	if(totalHeight > 210.0f)
	{
		ui::Text *prev = nullptr;
		/* everything needs to be smaller...
		 * required on KOR where everything is a tad larger */
		for(std::list<ui::BaseWidget *>::iterator it = q.bot.begin(); it != q.bot.end(); ++it)
		{
			ui::Text *label = (ui::Text *) *it;
			label->resize(size - 0.28f, size - 0.28f);
			++it;
			ui::Text *cur = (ui::Text *) *it;
			cur->resize(size - 0.13f, size - 0.13f);
			if(prev)
				label->set_y(ui::under(prev, label, 1.0f));
			cur->set_y(ui::under(label, cur, -2.0f));
			prev = cur;
		}
	}
}

void ui::CatMeta::setup(const hsapi::Category& cat)
{ this->set_cat(cat); }

void ui::CatMeta::set_cat(const hsapi::Category& cat)
{
	this->queue.clear();
	this->size = cat.meta.size;

	ui::builder<ui::Text>(this->screen, str::name)
		.size(0.40f)
		.x(this->get_x())
		.y(this->get_y())
		.add_to(this->queue);
	ui::builder<ui::Text>(this->screen, cat.disp)
		.size(0.70f)
		.x(this->get_x())
		.under(this->queue.back(), -1.0f)
		.scroll()
		.add_to(this->queue);

	PAIR_I18N(ui::human_readable_size_block(this->size), str::size,
		[this](ui::Text *t, lang::type) -> void {
			t->set_text(ui::human_readable_size_block(this->size));
		}
	);
	PAIR(std::to_string(cat.meta.titles), str::total_titles);
	PAIR(cat.desc, str::description);
	clip_q(this->queue, this->get_y());
}

bool ui::CatMeta::render(ui::Keys& keys)
{
	META_PANEL();
	return this->queue.render_screen(keys, this->screen);
}

float ui::CatMeta::width()
{ return 0.0f; } /* fullscreen */

float ui::CatMeta::height()
{ return 0.0f; } /* fullscreen */

float ui::CatMeta::get_y()
{ return 18.0f; }

float ui::CatMeta::get_x()
{ return 18.0f; }

/* SubMeta */

void ui::SubMeta::setup(const hsapi::Subcategory& sub)
{ this->set_sub(sub); }

void ui::SubMeta::set_sub(const hsapi::Subcategory& sub)
{
	this->queue.clear();
	this->size = sub.meta.size;

	ui::builder<ui::Text>(this->screen, str::name)
		.size(0.40f)
		.x(this->get_x())
		.y(this->get_y())
		.add_to(this->queue);
	ui::builder<ui::Text>(this->screen, sub.disp)
		.size(0.70f)
		.x(this->get_x())
		.under(this->queue.back(), -1.0f)
		.scroll()
		.add_to(this->queue);

	PAIR_I18N(ui::human_readable_size_block(this->size), str::size,
		[this](ui::Text *t, lang::type) -> void {
			t->set_text(ui::human_readable_size_block(this->size));
		}
	);
	PAIR(sub.parent->disp, str::category);
	PAIR(std::to_string(sub.meta.titles), str::total_titles);
	PAIR(sub.desc, str::description);
	clip_q(this->queue, this->get_y());
}

bool ui::SubMeta::render(ui::Keys& keys)
{
	META_PANEL();
	return this->queue.render_screen(keys, this->screen);
}

float ui::SubMeta::width()
{ return 0.0f; } /* fullscreen */

float ui::SubMeta::height()
{ return 0.0f; } /* fullscreen */

float ui::SubMeta::get_y()
{ return 18.0f; }

float ui::SubMeta::get_x()
{ return 18.0f; }

/* TitleMeta */

template <typename T>
void ui::TitleMeta::setup_with_title(const T& meta)
{
	this->queue.clear();

	this->size = meta.size;

	ui::builder<ui::Text>(this->screen, str::name)
		.size(0.40f)
		.x(this->get_x())
		.y(this->get_y())
		.add_to(this->queue);
	ui::builder<ui::Text>(this->screen, meta.name)
		.size(0.70f)
		.x(this->get_x())
		.under(this->queue.back(), -1.0f)
		.scroll()
		.add_to(this->queue);

	PAIR(hsapi::format_category_and_subcategory(meta.cat, meta.subcat), str::category);
	PAIR(meta.tid.to_string(), str::tid);
	PAIR(std::to_string(meta.id), str::landing_id);
	PAIR_I18N(ui::human_readable_size_block(meta.size), str::size,
		[this](ui::Text *t, lang::type) -> void {
			t->set_text(ui::human_readable_size_block(this->size));
		}
	);
	clip_q(this->queue, this->get_y());
}

void ui::TitleMeta::setup(const hsapi::PartialTitle& meta) { this->setup_with_title<hsapi::PartialTitle>(meta); }
void ui::TitleMeta::set_title(const hsapi::PartialTitle& meta) { this->setup_with_title<hsapi::PartialTitle>(meta); }
void ui::TitleMeta::setup(const hsapi::Title& meta) { this->setup_with_title<hsapi::Title>(meta); }
void ui::TitleMeta::set_title(const hsapi::Title& meta) { this->setup_with_title<hsapi::Title>(meta); }

bool ui::TitleMeta::render(ui::Keys& keys)
{
	META_PANEL();
	return this->queue.render_screen(keys, this->screen);
}

float ui::TitleMeta::width()
{ return 0.0f; } /* fullscreen */

float ui::TitleMeta::height()
{ return 0.0f; } /* fullscreen */

float ui::TitleMeta::get_y()
{ return 18.0f; }

float ui::TitleMeta::get_x()
{ return 18.0f; }
