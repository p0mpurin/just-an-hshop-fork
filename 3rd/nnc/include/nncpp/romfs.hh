
#ifndef inc_nnc_nncpp_romfs_hh
#define inc_nnc_nncpp_romfs_hh

#include <nncpp/base.hh>
#include <nncpp/utf.hh>
#include <nnc/romfs.h>


namespace nnc
{
	/* note: this wraps the "nnc_romfs_ctx" object instead of the raw "nnc_romfs_header" object */
	class romfs final
	{
	public:
		class info final
		{
		public:
			enum class itype { /* since C++ handles this a bit differently ... */
				file      = nnc_romfs_info::NNC_ROMFS_FILE,
				directory = nnc_romfs_info::NNC_ROMFS_DIR,
				none      = nnc_romfs_info::NNC_ROMFS_NONE,
			};

			bool is_directory() { return this->type() == itype::directory; }
			bool is_file() { return this->type() == itype::file; }

			NNCPP__DEFINE_ENUM_GETTER(type, this->info.type, enum nnc_romfs_info::nnc_romfs_type, itype)
			NNCPP__DEFINE_UTF16_VAR_SIZE_GETTER(name, this->info.filename, this->info.filename_length)

		private:
			friend class romfs;

			nnc_romfs_info info;

		};

		class children_iterator final : public iterator<info>
		{
		public:
			bool next(info& next_info)
			{
				return nnc_romfs_next(&this->it, &next_info.info) == 1;
			}

		private:
			children_iterator(nnc_romfs_ctx *ctx, const nnc_romfs_info *info)
				: it(nnc_romfs_mkit(ctx, info)) { }

			friend class romfs;

			nnc_romfs_iterator it;

		};

	public:
		romfs()
		{
			this->ctx.file_hash_tab = nullptr;
			this->ctx.dir_hash_tab = nullptr;
			this->ctx.file_meta_data = nullptr;
			this->ctx.dir_meta_data = nullptr;
		}
#if NNCPP_ALLOW_IGNORE_ERRORS
		romfs(read_stream_like& rs) { this->read(rs); }
#endif

		~romfs()
		{
			nnc_free_romfs(&this->ctx);
		}

		result read(read_stream_like& rs)
		{
			result res = (result) nnc_init_romfs(rs.as_rstream(), &this->ctx);
			if(res == result::ok)
				this->rsl = &rs;
			return res;
		}

		children_iterator children_for(const info& info)
		{
			return children_iterator { &this->ctx, &info.info };
		}

		info root()
		{
			info root;
			/* getting / should never fail */
			(void) this->get_info("/", root);
			return root;
		}

		result get_info(const char *path, info& out_info)
		{
			return (result) nnc_get_info(&this->ctx, &out_info.info, path);
		}

		result open(const char *path, subview& out)
		{
			info inf;
			result res = this->get_info(path, inf);
			if(res != result::ok) return res;
			return this->open(inf, out);
		}

		result open(info& finfo, subview& out)
		{
			result res = (result) nnc_romfs_open_subview(&this->ctx, out.csubstream(), &finfo.info);
			if(res == nnc::result::ok)
				out.set_open_state(true);
			return res;
		}

	private:
		read_stream_like *rsl = nullptr;
		nnc_romfs_ctx ctx;

	};
}

#endif

