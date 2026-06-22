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

#include "ui/confirm.hh"

#define END(r) { *this->ret = (r); return false; }

enum ctag {
	cancel,
	confirm,
	label
};

void ui::Confirm::setup(bool& ret)
{
	this->ret = &ret;

	ui::builder<ui::Button>(this->screen, str::confirm)
		.when_clicked([this](ui::Button *) END(true))
		.tag(ctag::confirm)
		.wrap()
		.add_to(this->queue);
	ui::builder<ui::Button>(this->screen, str::cancel)
		.when_clicked([this](ui::Button *) END(false))
		.tag(ctag::cancel)
		.wrap()
		.add_to(this->queue);
}

void ui::Confirm::setup(const std::string& label, bool& ret)
{
	this->setup(ret);
	this->set_label(label);
}

void ui::Confirm::setup(str::type labelid, bool& ret)
{
	this->setup(ret);
	this->set_label(labelid);
	this->i18n_auto_update_setup();
}

#define LABEL_BUILDER(lb) \
	ui::builder<ui::Text>(this->screen, lb) \
		.x(ui::layout::center_x) \
		.y(this->y) \
		.tag(ctag::label) \
		.wrap() \
		.add_to(this->queue);

void ui::Confirm::set_label(const std::string& label)
{
	ui::Text *prevlabel = this->queue.find_tag<ui::Text>(ctag::label);

	if (prevlabel != nullptr)
	{
		prevlabel->disable_i18n_update();
		prevlabel->set_text(label);
		this->adjust();
		return;
	}

	LABEL_BUILDER(label)

	this->adjust();
}

void ui::Confirm::set_label(str::type labelid)
{
	ui::Text *prevlabel = this->queue.find_tag<ui::Text>(ctag::label);

	if (prevlabel != nullptr)
	{
		prevlabel->set_text(labelid);
		this->labelid = labelid;
		this->adjust();
		return;
	}

	LABEL_BUILDER(labelid)

	this->adjust();
	this->labelid = labelid;
}

void ui::Confirm::adjust()
{
	ui::Button *yes = this->queue.find_tag<ui::Button>(ctag::confirm);
	ui::Button *no = this->queue.find_tag<ui::Button>(ctag::cancel);
	ui::Text *label = this->queue.find_tag<ui::Text>(ctag::label);

	label->set_y(this->y);

	float yl = yes->textwidth();
	float nl = no->textwidth();

	const float border = 6.0f;

	float largest = yl > nl ? yl : nl;
	float middle = (ui::screen_width(this->screen) / 2) - (largest / 2);

	yes->set_x(middle - largest / 2 - border);
	yes->set_y(ui::under(label, yes));
	yes->resize(largest + border, 20.0f);

	no->set_x(middle + largest / 2 + border);
	no->set_y(ui::under(label, no));
	no->resize(largest + border, 20.0f);
}

void ui::Confirm::set_y(float y)
{
	this->y = ui::transform(this, y);
	this->adjust();
}

bool ui::Confirm::render(ui::Keys& keys)
{
	if(keys.kDown & (KEY_B | KEY_A))
		END(keys.kDown & KEY_A);

	return this->queue.render_screen(keys, this->screen);
}

float ui::Confirm::height()
{
	return 20.0f + this->queue.find_tag(ctag::label)->height();
}

float ui::Confirm::width()
{
	return (this->queue.back()->width() * 2) + 6.0f;
}

#define LABEL_BUILDER_TOP(lb_top,rq) \
	ui::builder<ui::Text>(ui::Screen::top, lb_top) \
		.x(ui::layout::center_x) \
		.y(ui::layout::base) \
		.wrap() \
		.add_to(rq);

#define CONFIRM_BUILDER(lb,rq) \
	ui::builder<ui::Confirm>(ui::Screen::bottom, lb, ret) \
		.y(ui::layout::center_y) \
		.add_to(rq);

bool ui::Confirm::exec(const std::string& label, const std::string& label_top, bool ret)
{
	ui::RenderQueue queue;

	if(label_top.size())
	{
		LABEL_BUILDER_TOP(label_top, queue)
	}

	CONFIRM_BUILDER(label, queue)

	queue.render_finite();
	return ret;
}

bool ui::Confirm::exec(str::type labelid, str::type label_top_id, bool ret)
{
	ui::I18NEnabledRenderQueue queue;

	if (label_top_id != str::_i_max)
	{
		LABEL_BUILDER_TOP(label_top_id, queue)
	}

	CONFIRM_BUILDER(labelid, queue)

	queue.render_finite();
	return ret;
}

#undef CONFIRM_BUILDER
#undef LABEL_BUILDER_TOP

void ui::Confirm::i18n_auto_update_setup()
{
	this->i18n_update_cb = [this](lang::type nlang) -> void {
		this->queue.find_tag(ctag::confirm)->i18n_update(nlang);
		this->queue.find_tag(ctag::cancel)->i18n_update(nlang);
		this->set_label(this->labelid);
		this->adjust();
	};
	this->does_i18n_update = true;
}
