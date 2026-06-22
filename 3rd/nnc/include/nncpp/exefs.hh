
#ifndef inc_nnc_nncpp_exefs_hh
#define inc_nnc_nncpp_exefs_hh

#include <nncpp/base.hh>
#include <nncpp/stream.hh>
#include <nncpp/crypto.hh>
#include <nnc/exefs.h>
#include <string>


namespace nnc
{
	namespace detail
	{
		template <typename TStream>
		class exefs_base
		{
		public:
			using stream = TStream;

			class file_header final
			{
			public:
				operator nnc_exefs_file_header* () { return &this->fh; }

				NNCPP__DEFINE_ARRAY_GETTER_SETTER(name, this->fh.name, byte_array<9>)
				NNCPP__DEFINE_GETTER_SETTER(offset, this->fh.offset, u32)
				NNCPP__DEFINE_GETTER_SETTER(size, this->fh.size, u32)
				NNCPP__DEFINE_ARRAY_GETTER_SETTER(hash, this->fh.hash, sha256)

			private:
				nnc_exefs_file_header fh;

			};
			NNCPP__MUST_WRAP(file_header, nnc_exefs_file_header)

		public:
			file_header *find(const std::string& name) { return this->find(name.c_str()); }
			file_header *find(const char *name)
			{
				i8 index = nnc_find_exefs_file_index(name, (nnc_exefs_file_header *) this->fhs);
				return index == -1 ? nullptr : &this->fhs[index];
			}

			result open_file(const std::string& name, TStream& out)
			{
				file_header *hdr = this->find(name);
				return hdr ? this->open_file_impl(*hdr, out) : result::not_found;
			}

			result open_file(const char *name, TStream& out)
			{
				file_header *hdr = this->find(name);
				return hdr ? this->open_file_impl(*hdr, out) : result::not_found;
			}

			result open_file(file_header& hdr, TStream& out)
			{
				return this->open_file_impl(hdr, out);
			}

		protected:
			virtual result open_file_impl(file_header& hdr, TStream& out) = 0;

			file_header fhs[NNC_EXEFS_MAX_FILES];

		};
	}

	class exefs final : public detail::exefs_base<subview>
	{
	public:
		exefs() { }
#if NNCPP_ALLOW_IGNORE_ERRORS
		exefs(read_stream_like& rs) { this->read(rs); }
#endif
		result read(read_stream_like& rs)
		{
			result res = (result) nnc_read_exefs_header(rs.as_rstream(), (nnc_exefs_file_header *) this->fhs, nullptr);
			if(res == result::ok)
				this->rsl = &rs;
			return res;
		}

	protected:
		result open_file_impl(file_header& hdr, subview& out) override
		{
			nnc_exefs_subview(this->rsl->as_rstream(), out.csubstream(), hdr);
			out.set_open_state(true);
			return result::ok;
		};

	private:
		read_stream_like *rsl = nullptr;

	};
}

#endif

