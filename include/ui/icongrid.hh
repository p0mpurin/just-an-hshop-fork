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

#ifndef inc_ui_icongrid_hh
#define inc_ui_icongrid_hh

#include "sync.hh"
#include <ui/base.hh>


namespace ui
{
	template <typename T>
	class IconGrid : public ui::BaseWidget
	{ UI_WIDGET("IconGrid")
	public:
		static constexpr size_t selection_disabled = (size_t) -1;
		static constexpr float auto_arranged_padding = 8.0f;
		static constexpr float row_padding = 6.0f;
		static constexpr float border_size = 1.0f;

		using on_select_type = std::function<bool(IconGrid<T> *, size_t, u32)>;
		using on_change_type = std::function<void(IconGrid<T> *, size_t)>;
		using on_load_more_type = std::function<bool(IconGrid<T> *)>;

		UI_BUILDER_EXTENSIONS()
		{
			UI_USING_BUILDER()
		public:
			ReturnValue automatically_arranged()
			{
				this->instance().flags |= autosize_columns;
				this->instance().update_column_count_state();
				return this->return_value();
			}

			ReturnValue selection()
			{
				this->instance().selected = 0;
				return this->return_value();
			}

			ReturnValue column_count(u8 val)
			{
				this->instance().colcount = val;
				this->instance().update_column_count_state();
				return this->return_value();
			}

			ReturnValue icon_dimensions(u8 w, u8 h)
			{
				this->instance().set_dimensions(w, h);
				return this->return_value();
			}

			ReturnValue icon_dimensions(u8 wh)
			{
				return this->icon_dimensions(wh, wh);
			}

			ReturnValue buttons(u32 k)
			{
				this->instance().keys |= k;
				return this->return_value();
			}

			ReturnValue when_changed(on_change_type cb)
			{
				this->instance().on_change_ = cb;
				return this->return_value();
			}

			ReturnValue when_selected(on_select_type cb)
			{
				this->instance().on_select_ = cb;
				return this->return_value();
			}

			ReturnValue when_more_requested(on_load_more_type cb)
			{
				this->instance().on_load_more_ = cb;
				return this->return_value();
			}
		};

		void setup()
		{
			this->loadctx.set_icon_grid(this);
		}

		bool render(ui::Keys& keys) override
		{
			float xpos, ypos;

			if(this->selected != IconGrid::selection_disabled)
			{
				if(keys.kDown & KEY_DOWN)
				{
					if(this->selected + this->colcount < this->images.size())
					{
						this->set_selection_rel(+this->colcount);
						size_t minin = (this->rowshift + this->rowscreencount - 1) * this->colcount;
						size_t maxin = minin + this->colcount;
						if(this->selected >= minin && this->selected < maxin)
							++this->rowshift;
					}
				}
				if(keys.kDown & KEY_UP)
				{
					if(this->selected >= this->colcount)
					{
						this->set_selection_rel(-this->colcount);
						if(this->rowshift > 0)
						{
							size_t minin = (this->rowshift - 1) * this->colcount;
							size_t maxin = minin + this->colcount;
							if(this->selected >= minin && this->selected < maxin)
								--this->rowshift;
						}
					}
				}
				if(keys.kDown & KEY_RIGHT)
				{
					if(this->selected + 1 < this->images.size())
					{
						this->set_selection_rel(+1);
						size_t nextrowfirst = (this->rowshift + this->rowscreencount - 1) * this->colcount;
						if(this->selected == nextrowfirst)
							++this->rowshift;
					}
				}
				if(keys.kDown & KEY_LEFT)
				{
					if(this->selected > 0)
					{
						this->set_selection_rel(-1);
						size_t prevrowlast = this->rowshift * this->colcount - 1;
						if(this->selected == prevrowlast)
							--this->rowshift;
					}
				}
			}

			u32 i = this->rowshift * this->colcount;
			u32 iters = i + this->rowscreencount * this->colcount;
			if(iters > this->images.size())
				iters = this->images.size();

			for(u32 row = 0, col = 0; i < iters; ++i, ++col)
			{
				if(col == this->colcount)
				{
					col = 0;
					++row;
				}

				/* Now we draw the image */
				xpos = this->startx + col * this->colwidth + IconGrid::border_size;
				ypos = this->y + row * this->rowheight + IconGrid::border_size;

				/* This is not completely right...? */
				C2D_DrawRectSolid(xpos - IconGrid::border_size, ypos - IconGrid::border_size, this->z, this->iconsw + 2*IconGrid::border_size, this->iconsh + 2*IconGrid::border_size, i == this->selected ? 0xFF00FF00 : 0xFF000000);
				/* TODO: Should we do any size assertions here? */
				C2D_DrawImageAt(this->images[i].get_image(this->loadctx), xpos, ypos, this->z);
			}

			return this->selected != selection_disabled && (keys.kDown & this->keys)
				? this->on_select_(this, this->selected, keys.kDown) : true;
		}

		float height() override
		{
			/* not entirely correct... oh well, good enough */
			unsigned rows = this->images.size() / this->colcount;
			return rows * this->rowheight;
		}

		float width() override
		{
			return this->available_width();
		}

		void set_x(float val) override
		{
			this->x = ui::transform(this, val);
			this->update_column_count_state();
		}

		template <typename ... Args>
		size_t emplace_back(Args&&... args)
		{
			size_t ret = this->images.size();
			this->images.emplace_back(std::forward<Args>(args)...);
			return ret;
		}

		size_t column_count() { return this->colcount; }
		size_t size() { return this->images.size(); }

		typename T::LoadContext& loader()
		{
			return this->loadctx;
		}

		T& provider_at(size_t i)
		{
			return this->images[i];
		}

	private:
		std::vector<T> images;
		enum flag {
			autosize_columns = 1,
			load_completed   = 2,
			blocking_load    = 4,
			load_due         = 8,
		};

		on_select_type on_select_ = [](IconGrid<T> *, size_t, u32) -> bool { return true; };
		on_change_type on_change_ = [](IconGrid<T> *, size_t) -> void { };
		on_load_more_type on_load_more_ = [](IconGrid<T> *) -> bool { return true; };

		u8 colcount = 0, rowscreencount = 0, iconsw, iconsh, flags = 0;
		float startx, colwidth, rowheight;
		size_t selected = IconGrid::selection_disabled;
		typename T::LoadContext loadctx;
		size_t rowshift = 0;
		u32 keys = KEY_A;

		void set_selection_rel(int mod)
		{
			this->selected += mod;
			this->on_change_(this, this->selected);
			/* TODO: check if we should load more */
			if(!(this->flags & load_completed))
			{
				if(this->flags & blocking_load)
					this->flags |= load_due;
				else
					this->flags |= load_completed * this->on_load_more_(this);
			}
		}

		float available_width()
		{
			return ui::screen_width(this->screen) - (2*this->x);
		}

		float available_height()
		{
			return ui::screen_height() - this->y;
		}

		void update_column_count_state()
		{
			/* We need to recalculate the amount of columns we can use */
			if(this->flags & autosize_columns)
			{
				this->colcount = this->available_width() / (float) (this->iconsw + 2 * IconGrid::border_size + IconGrid::auto_arranged_padding);
			}
			/* Now we need to calculate how large one column will be
			 * given the specified column count */
			this->colwidth = this->available_width() / this->colcount;
			/* Now we can calculate where we should start drawing
			 * given the specified column width */
			/* TODO: Fix custom column width */
			float borderpadding = (this->available_width() - this->colwidth * this->colcount) / 2.0f;
			this->startx = this->x + borderpadding;
		}

		void set_dimensions(u8 w, u8 h)
		{
			this->iconsw = w;
			this->iconsh = h;
			this->update_column_count_state();
			/* rowheight is only dependant on the image height */
			this->rowheight = this->iconsh + 2 * IconGrid::border_size + IconGrid::row_padding;
			this->rowscreencount = (this->available_height() / this->rowheight) + 1;
		}

	};

	template <typename Loader>
	class LazyImageProvider
	{
	public:
		class LoadContext {
			friend class LazyImageProvider;

		public:
			LoadContext() :
				signal_complete(ctr::Event::ResetType::Oneshot),
				signal_work(ctr::Event::ResetType::Oneshot)
			{
				s32 prio = 0;
				svcGetThreadPriority(&prio, CUR_THREAD_HANDLE);
				this->workthread = threadCreate(&LoadContext::_entrypoint, this, 64 * 1024, prio + 1, -2, false);
				panic_assert(this->workthread, "faild to create thread");
			}

			~LoadContext()
			{
				if(this->current_load)
					this->signal_complete.wait();
				this->signal_work.signal();
				threadJoin(this->workthread, U64_MAX);
				threadFree(this->workthread);
			}

			void set_icon_grid(IconGrid<LazyImageProvider<Loader>> *) { }

			void set_loading_image(C2D_Image img)
			{
				this->loadimg = img;
			}

		private:
			LazyImageProvider *current_load = nullptr;
			C2D_Image loadimg;
			Thread workthread;

			ctr::Event signal_complete;
			ctr::Event signal_work;
			ctr::Lock lock;

			static void _entrypoint(void *arg)
			{
				LoadContext *me = (LoadContext *) arg;

				while(1)
				{
					me->signal_work.wait();
					/* We're done with this thread; we can exit now */
					if(!me->current_load) break;
					me->current_load->perform_load();
					me->current_load = nullptr;
					me->signal_complete.signal();
				}
			}

			bool is_available_for_work()
			{
				return !current_load;
			}

			bool is_loading(LazyImageProvider *me)
			{
				return this->current_load == me;
			}

			void enqueue_image_load(LazyImageProvider *me)
			{
				this->current_load = me;
				this->signal_work.signal();
			}

		};

	public:
		LazyImageProvider(typename Loader::DataTypeRef data)
			: data_st(data) { }

		~LazyImageProvider()
		{
			if(this->has_image_loaded())
				Loader::unload(this->img);
		}

		C2D_Image get_image(LoadContext& ctx)
		{
			if(ctx.is_loading(this))
				return ctx.loadimg;

			if(!this->has_image_loaded())
			{
				if(ctx.is_available_for_work())
					ctx.enqueue_image_load(this);
				return ctx.loadimg;
			}

			return this->img;
		}

		typename Loader::DataTypeRef data()
		{
			return this->data_st;
		}

	private:
		C2D_Image img = {nullptr, nullptr};
		typename Loader::DataType data_st;

		bool has_image_loaded()
		{
			return !!this->img.tex;
		}

		void perform_load()
		{
			Loader::load(this->data(), &this->img);
			panic_assert(this->img.tex, "failed to load image");
		}

	};

	template <typename Loader>
	class BatchLazyImageProvider
	{
	public:
		/* This context differs from the LoadContext in that it's meant for to provide functions to the user callback,
		 * it's thus not used by IconGrid like LoadContext but rathre by the user callbacks in Loader */
		class Context
		{
			friend class BatchLazyImageProvider;

		public:
			typename Loader::DataTypeRef data_for(size_t id)
			{
				return this->grid->provider_at(id).data();
			}

			void loaded_for(size_t id, C2D_Image img)
			{
				/* would a panic_assert() be better? */
				if(!img.tex) return;

				BatchLazyImageProvider& prov = this->grid->provider_at(id);
				prov.img = img;
				prov.is_loaded = true;
			}

			bool should_die()
			{
				return this->grid->loader().should_die();
			}

		private:
			IconGrid<BatchLazyImageProvider<Loader>> *grid;

			void set_icon_grid(IconGrid<BatchLazyImageProvider<Loader>> *grid)
			{
				this->grid = grid;
			}

		};

	public:
		class LoadContext {
			friend class BatchLazyImageProvider;
			using Container = std::list<size_t>;

		public:
			LoadContext()
			{
				s32 prio = 0;
				svcGetThreadPriority(&prio, CUR_THREAD_HANDLE);
				this->workthread = threadCreate(&LoadContext::_entrypoint, this, 64 * 1024, prio + 1, -2, false);
				panic_assert(this->workthread, "faild to create thread");
			}

			~LoadContext()
			{
				this->kill();
				threadJoin(this->workthread, U64_MAX);
				threadFree(this->workthread);
			}

			void set_icon_grid(IconGrid<BatchLazyImageProvider<Loader>> *grid)
			{
				this->tctx.set_icon_grid(grid);
			}

			void set_loading_image(C2D_Image img)
			{
				this->loadimg = img;
			}

			void add_to_batch(size_t id)
			{
				ctr::LockedInScope l { this->lock };
				this->next_queue().push_back(id);
			}

			void process_batch()
			{
				ctr::LockedInScope l { this->lock };

				if(this->flags & flag_processing)
				{
					/* If we're already processing then we process this next batch
					 * whenever the current one is finished */
					this->flags |= flag_run_after_complete;
					return;
				}
				/* otherwise we start the job */
				this->swap_and_clear_queue();
				this->event.signal();
			}

		private:
			static void _entrypoint(void *arg)
			{
				LoadContext *me = (LoadContext *) arg;

				while(1)
				{
					me->event.wait();
					{
						ctr::LockedInScope l { me->lock };
						if(me->should_die()) break;
						me->flags |= flag_processing;
					}
start_proc:
					/* Run batch with this->processing_queue() */
					Loader::batch_load(me->processing_queue(), me->tctx);

					/* Potentially re-run if flag_run_after_complete and otherwise unset the
					 * processing flag */
					me->lock.lock();
					if((me->flags & flag_run_after_complete) && !me->should_die())
					{
						me->flags &= ~flag_run_after_complete;
						me->swap_and_clear_queue();
						me->lock.unlock();
						goto start_proc;
					}
					me->flags &= ~flag_processing;
					me->lock.unlock();
				}
			}

			/* NOTE: The caller must acquire the lock */
			void swap_and_clear_queue()
			{
				this->processing_queue().clear();
				this->qsel ^= 1;
			}

			void kill()
			{
				ctr::LockedInScope l { this->lock };
				this->flags |= flag_kill;
				this->event.signal();
			}

			bool should_die()
			{
				return this->flags & flag_kill;
			}

			Container& processing_queue() { return this->qds[this->qsel ^ 0]; };
			Container& next_queue() { return this->qds[this->qsel ^ 1]; };

			enum flag {
				flag_processing         = 1,
				flag_run_after_complete = 2,
				flag_kill               = 4,
			};

			ctr::Event event { ctr::Event::ResetType::Oneshot };
			C2D_Image loadimg;
			Thread workthread;
			Container qds[2];
			ctr::Lock lock;
			Context tctx;
			int qsel = 0;
			u8 flags = 0;
		};

	public:
		BatchLazyImageProvider(typename Loader::DataTypeRef data)
			: data_st(data) { }

		~BatchLazyImageProvider()
		{
			if(this->is_loaded)
				Loader::unload(this->img);
		}

		C2D_Image get_image(LoadContext& ctx)
		{
			return this->is_loaded ? this->img : ctx.loadimg;
		}

		typename Loader::DataTypeRef data()
		{
			return this->data_st;
		}

	private:
		typename Loader::DataType data_st;
		bool is_loaded = false;
		C2D_Image img;

	};

	class ImageProvider
	{
	public:
		class LoadContext {
		public:
			void set_icon_grid(IconGrid<ImageProvider> *) { }
			/* no data required */
		};
	public:
		ImageProvider(const C2D_Image& imgb) : img(imgb) {}
		C2D_Image get_image(LoadContext&) { return this->img; }

	private:
		C2D_Image img;

	};
}

#endif

