/** \file  cia.hh
 *  \see   \ref cia.h
 */
#ifndef inc_nnc_nncpp_cia_hh
#define inc_nnc_nncpp_cia_hh

#include <nncpp/base.hh>
#include <nncpp/stream.hh>
#include <nncpp/tmd.hh>
#include <nnc/cia.h>

/* \cond INTERNAL */
#define NNCPP__DEFINE_CIA_SECTION(name, open_func) \
	void name##_view(subview& sv) \
	{ \
		open_func(&this->hdr, this->rsl->as_rstream(), sv.csubstream()); \
		sv.set_open_state(true); \
	} \
	subview name##_view() \
	{ \
		subview ret; \
		this->name##_view(ret); \
		return ret; \
	}
/* \endcond */


namespace nnc
{
	class cia final
	{
	public:
		class content_index_iterator final : public iterator<u32>
		{
		public:
			bool next(u32& next_index)
			{
				for(; this->i != 0x2000; ++this->i)
				{
					for(; this->j != -1; --this->j)
					{
						if(this->own->hdr.content_index[this->i] & (1 << this->j))
						{
							next_index = 8 * this->i + (7 - this->j);
							--this->j;
							return true;
						}
					}
					this->j = 7;
				}
				return false;
			}

		private:
			content_index_iterator(cia *from)
				: own(from), i(0), j(7) { }

			friend class cia;

			cia *own;
			int i, j;

		};

	public:
#if NNCPP_ALLOW_IGNORE_ERRORS
		cia(read_stream_like& rs) { this->read(rs); }
#endif
		cia() { }

		result read(read_stream_like& rs)
		{
			result r = (result) nnc_read_cia_header(rs.as_rstream(), &this->hdr);
			if(r == result::ok)
				this->rsl = &rs;
			return r;
		}

		NNCPP__DEFINE_GETTER_SETTER(type, this->hdr.type, u16)
		NNCPP__DEFINE_GETTER_SETTER(version, this->hdr.version, u16)
		NNCPP__DEFINE_GETTER_SETTER(certificate_chain_size, this->hdr.cert_chain_size, u32)
		NNCPP__DEFINE_GETTER_SETTER(ticket_size, this->hdr.ticket_size, u32)
		NNCPP__DEFINE_GETTER_SETTER(tmd_size, this->hdr.tmd_size, u32)
		NNCPP__DEFINE_GETTER_SETTER(meta_size, this->hdr.meta_size, u32)
		NNCPP__DEFINE_GETTER_SETTER(content_size, this->hdr.content_size, u64)

		bool has_in_cindex(u16 indx) { return NNC_CINDEX_HAS(this->hdr.content_index, indx); }
		content_index_iterator cindex() { return content_index_iterator { this }; }
		bool operator [] (u16 indx) { return this->has_in_cindex(indx); }

		NNCPP__DEFINE_CIA_SECTION(certificate_chain, nnc_cia_open_certchain)

		/* TODO: Implement certificate chain and ctor here */

		NNCPP__DEFINE_CIA_SECTION(ticket, nnc_cia_open_ticket)

		/* TODO: Implement ticket and ctor here */

		NNCPP__DEFINE_CIA_SECTION(tmd, nnc_cia_open_tmd)
		result tmd(nnc::tmd_header& hdr)
		{
			nnc::subview sv;
			this->tmd_view(sv);
			return hdr.read(sv);
		}

#if NNCPP_ALLOW_IGNORE_ERRORS
		tmd_header tmd()
		{
			subview sv = this->tmd_view();
			return { sv };
		}
#endif

		bool has_meta_section() { return this->meta_size() != 0; }
		result meta_view(subview& sv)
		{
			result r = (result) nnc_cia_open_meta(&this->hdr, this->rsl->as_rstream(), sv.csubstream());
			if(r == result::ok)
				sv.set_open_state(true);
			return r;
		}
#if NNCPP_ALLOW_IGNORE_ERRORS
		subview meta_view()
		{
			subview ret;
			this->meta_view(ret);
			return ret;
		}
#endif

		/* TODO: Implement meta generally */

	private:
		struct nnc_cia_header hdr;
		read_stream_like *rsl = nullptr;

		friend class cia_reader;
	};

	class cia_content final : public c_read_stream<nnc_cia_content_stream>
	{ public: using c_read_stream::c_read_stream; };

	class cia_reader final
	{
	public:
		~cia_reader()
		{
			if(this->creader.chunks)
				nnc_cia_free_reader(&this->creader);
		}

		cia_reader() { this->creader.chunks = nullptr; }
#if NNCPP_ALLOW_IGNORE_ERRORS
		cia_reader(cia& c, keyset::param kset = keyset::default_value())
		{
			this->initialize(c, kset);
		}
#endif

		result initialize(cia& c, keyset::param kset = keyset::default_value())
		{
			return (result) nnc_cia_make_reader(&c.hdr, c.rsl->as_rstream(), kset, &this->creader);
		}

		result open(u16 indx, cia_content& out_content, chunk_record *out_chunk = nullptr)
		{
			nnc_chunk_record *cchunk;
			result res = (result) nnc_cia_open_content(&this->creader, indx, (nnc_cia_content_stream *) out_content.as_rstream(), &cchunk);
			if(res == result::ok)
			{
				if(out_chunk) out_chunk->record = *cchunk;
				out_content.set_open_state(true);
			}
			return res;
		}

	private:
		struct nnc_cia_content_reader creader;

	};
}

#endif

