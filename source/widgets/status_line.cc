#include "widgets/status_line.hh"

#define SLINE_MOD 0.5f

void ui::StatusLine::reset()
{
	if(this->flags & ui::StatusLine::flag_running)
	{
		ui::RenderQueue::global()->find_tag(ui::tag::net_indicator)->set_hidden(!!(this->flags & ui::StatusLine::flag_net_is_hidden));
		ui::RenderQueue::global()->find_tag(ui::tag::free_indicator)->set_hidden(!!(this->flags & ui::StatusLine::flag_free_is_hidden));
		this->text.destroy();
	}
	this->flags = 0;
}

void ui::StatusLine::start(const std::string& str, ui::StatusLine::StatusDisplayType type)
{
	this->text.setup(this->screen, str);
	this->text->resize(0.35f, 0.35f);
	this->text->set_raw_y(ui::screen_height() - 10.0f);
	this->fadeoutx = -this->text->width() - 10.0f;
	this->text.finalize();

	switch(type)
	{
	case ui::StatusLine::StatusDisplayType::slide_in:
		this->xpos = this->fadeoutx;
		this->text->set_raw_x(this->xpos);
		break;
	case ui::StatusLine::StatusDisplayType::ticker:
		this->xpos = ui::screen_width(this->screen) + 10.0f;
		this->text->set_raw_x(this->xpos);
		this->flags |= ui::StatusLine::flag_is_ticker;
		break;
	}

	ui::BaseWidget *w = ui::RenderQueue::global()->find_tag(ui::tag::net_indicator);
	this->flags |= w->is_hidden() ? ui::StatusLine::flag_net_is_hidden : 0;
	w->set_hidden(true);

	w = ui::RenderQueue::global()->find_tag(ui::tag::free_indicator);
	this->flags |= w->is_hidden() ? ui::StatusLine::flag_free_is_hidden : 0;
	w->set_hidden(true);

	this->flags |= ui::StatusLine::flag_running;
}

void ui::StatusLine::ticker(const std::string& str)
{
	this->start(str, ui::StatusLine::StatusDisplayType::ticker);
}

void ui::StatusLine::run(const std::string& str)
{
	this->start(str, ui::StatusLine::StatusDisplayType::slide_in);
}

bool ui::StatusLine::render(ui::Keys& keys)
{
	if(!(this->flags & ui::StatusLine::flag_running))
		return true;

	this->text->render(keys);

	/* we need to run in "ticker mode"; scroll text in screen once from right to left */
	if(this->flags & ui::StatusLine::flag_is_ticker)
	{
		this->xpos -= SLINE_MOD;
		this->text->set_raw_x(this->xpos);
		if(this->xpos < this->fadeoutx)
			this->reset();
	}
	/* wait 2 seconds when we get in position */
	else if(this->flags & ui::StatusLine::flag_is_in_position)
	{
		if(time(NULL) - this->in_pos_start > 2)
		{
			this->flags &= ~StatusLine::flag_is_in_position;
			this->flags |= ui::StatusLine::flag_return_in_progress;
		}
	}
	/* return to -this->text->width() - 10.0f */
	else if(this->flags & ui::StatusLine::flag_return_in_progress)
	{
		this->xpos -= SLINE_MOD;
		this->text->set_raw_x(this->xpos);
		if(this->xpos < this->fadeoutx)
			this->reset();
	}
	/* else we must progress to 5.0f */
	else
	{
		this->xpos += SLINE_MOD;
		this->text->set_raw_x(this->xpos);
		if(this->xpos > 5.0f)
		{
			this->in_pos_start = time(NULL);
			this->flags |= ui::StatusLine::flag_is_in_position;
		}
	}

	return true;
}
