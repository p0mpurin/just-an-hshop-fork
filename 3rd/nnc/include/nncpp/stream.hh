
#ifndef inc_nnc_nncpp_stream_hh
#define inc_nnc_nncpp_stream_hh

#include <nncpp/base.hh>
#include <nnc/stream.h>
#include <string>


namespace nnc
{
	class read_stream_like
	{
	public:
		nnc_rstream *as_rstream() { return this->cstream(); }
		virtual nnc_rstream *cstream() = 0;

		virtual result read(void *buf, u32 max, u32& totalRead) = 0;
		virtual result seek_abs(u32 pos) = 0;
		virtual result seek_rel(u32 offset) = 0;
		virtual u32 size() = 0;
		virtual u32 tell() = 0;
		virtual void close() = 0;

		template <size_t S> result read(byte_array<S>& barr, u32 maxlen, u32& totalRead) { return this->read(barr.data(), maxlen, totalRead); }
		template <size_t S> result read(byte_array<S>& barr, u32& totalRead) { return this->read(barr.data(), barr.size(), totalRead); }
		template <typename T> result read(span<T>& spn, u32 maxlen, u32& totalRead) { return this->read(spn.data(), maxlen, totalRead); }
		template <typename T> result read(span<T>& spn, u32& totalRead) { return this->read(spn.data(), spn.size(), totalRead); }

	};

	template <typename CStreamType>
	class c_read_stream : public read_stream_like
	{
	public:
		using read_stream_like::read_stream_like::read;

		c_read_stream(CStreamType& cstream)
			: stream(cstream) { }
		c_read_stream() { }

		~c_read_stream() { this->close(); }

		nnc_rstream *cstream() override { return (nnc_rstream *) &this->stream; }
		CStreamType *csubstream() { return &this->stream; }

		result read(void *buf, u32 max, u32& totalRead) override { return (nnc::result) this->cstream()->funcs->read(this->cstream(), (u8 *) buf, max, &totalRead); }
		result seek_abs(u32 pos) override { return (nnc::result) this->cstream()->funcs->seek_abs(this->cstream(), pos); }
		result seek_rel(u32 offset) override { return (nnc::result) this->cstream()->funcs->seek_rel(this->cstream(), offset); }
		u32 tell() override { return this->cstream()->funcs->tell(this->cstream()); }
		u32 size() override { return this->cstream()->funcs->size(this->cstream()); }
		void close() override
		{
			if(this->is_open())
			{
				this->cstream()->funcs->close(this->cstream());
				this->set_open_state(false);
			}
		}

		void set_open_state(bool b) { this->open_state = b; }
		bool is_open()              { return this->open_state; }

	protected:
		CStreamType stream;
		bool open_state = false; /* XXX: Should this be enforced in other actions than close as well? */

	};

	class file final : public c_read_stream<nnc_file>
	{
	public:
		using c_read_stream::c_read_stream;

#if NNCPP_ALLOW_IGNORE_ERRORS
		file(const std::string& filename) { this->open(filename); }
		file(const char *filename) { this->open(filename); }
#endif

		result open(const std::string& filename) { return this->open(filename.c_str()); }

		result open(const char *filename)
		{
			/* ensure the file is closed */
			this->close();
			result ret = (result) nnc_file_open(&this->stream, filename);
			if(ret == nnc::result::ok)
				this->set_open_state(true);
			return ret;
		}
	};

	class subview final : public c_read_stream<nnc_subview>
	{
	public:
		using c_read_stream::c_read_stream;
		subview(nnc_rstream *child, u32 offset, u32 len)
		{
			this->open(child, offset, len);
		}

		subview(read_stream_like& child, u32 offset, u32 len)
		{
			this->open(child, offset, len);
		}

		void open(read_stream_like& child, u32 offset, u32 len)
		{
			this->open(child.as_rstream(), offset, len);
		}

		void open(nnc_rstream *child, u32 offset, u32 len)
		{
			this->close();
			nnc_subview_open(&this->stream, child, offset, len);
			this->set_open_state(true);
		}
	};

	class memory final : public c_read_stream<nnc_memory>
	{
	public:
		using c_read_stream::c_read_stream;
		memory(const void *ptr, u32 size)
		{
			this->open(ptr, size);
		}

		template <size_t N>
		memory(byte_array<N>& barr)
		{
			this->open(barr);
		}

		template <size_t N>
		void open(byte_array<N>& barr)
		{
			this->open(barr.data(), N);
		}

		void open(const void *ptr, u32 size)
		{
			nnc_mem_open(&this->stream, ptr, size);
			this->set_open_state(true);
		}
	};

	/* interface for a custom read stream */
	class read_stream : public read_stream_like
	{
	private:
		struct wrapper_rstream {
			const nnc_rstream_funcs *funcs;
			read_stream *self;
		};

		static cresult c_read(nnc_rstream *obj, u8 *buf, u32 max, u32 *totalRead) { return (cresult) ((wrapper_rstream *) obj)->self->read(buf, max, *totalRead); }
		static cresult c_seek_abs(nnc_rstream *obj, u32 pos) { return (cresult) ((wrapper_rstream *) obj)->self->seek_abs(pos); }
		static cresult c_seek_rel(nnc_rstream *obj, u32 offset) { return (cresult) ((wrapper_rstream *) obj)->self->seek_rel(offset); }
		static u32 c_size(nnc_rstream *obj) { return ((wrapper_rstream *) obj)->self->size(); }
		static void c_close(nnc_rstream *obj) { ((wrapper_rstream *) obj)->self->close(); }
		static u32 c_tell(nnc_rstream *obj) { return ((wrapper_rstream *) obj)->self->tell(); }

		/* storing this as a member when all members are constant is suboptimal but oh well
		 *  maybe some day i'll think of a better way to do this */
		const nnc_rstream_funcs c_funcs = {
			c_read, c_seek_abs, c_seek_rel,
			c_size, c_close, c_tell,
		};

	public:
		using read_stream_like::read_stream_like::read;

		virtual result read(void *buf, u32 max, u32& totalRead) = 0;
		virtual result seek_abs(u32 pos) = 0;
		virtual result seek_rel(u32 offset) = 0;
		virtual u32 size() = 0;
		virtual void close() = 0;
		virtual u32 tell() = 0;

	protected:
		nnc_rstream *cstream() override
		{
			return (nnc_rstream *) &this->stream;
		}

	private:
 		wrapper_rstream stream = { &c_funcs, this };

	};
}

#endif

