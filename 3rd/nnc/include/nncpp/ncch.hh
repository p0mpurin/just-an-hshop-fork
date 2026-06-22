
#ifndef inc_nnc_nncpp_ncch_hh
#define inc_nnc_nncpp_ncch_hh

#include <nncpp/stream.hh>
#include <nncpp/crypto.hh>
#include <nncpp/romfs.hh>
#include <nncpp/exefs.hh>
#include <nncpp/base.hh>
#include <nncpp/u128.hh>
#include <nnc/ncch.h>

/* \cond INTERNAL */
#define NNCPP__DEFINE_NCCH_SECTION(name, open_func) \
	result name##_section(ncch_section& sect) \
	{ \
		result r = (result) open_func(&this->hdr, this->rsl->as_rstream(), &this->kpair.pair, sect.csubstream()); \
		if(r == result::ok) \
			sect.set_open_state(true); \
		return r; \
	}

#if NNCPP_ALLOW_IGNORE_ERRORS
#define NNCPP__DEFINE_SUBVIEW_SECTION_DIRECT(name) \
	subview name##_section() \
	{ \
		subview ret; \
		this->name##_section(ret); \
		return ret; \
	}
#else
	#define NNCPP__DEFINE_SUBVIEW_SECTION_DIRECT(name)
#endif

#define NNCPP__DEFINE_SUBVIEW_SECTION(name, open_func) \
	result name##_section(subview& sv) \
	{ \
		result r = (result) open_func(&this->hdr, this->rsl->as_rstream(), (nnc_subview *) sv.as_rstream()); \
		if(r == result::ok) \
			sv.set_open_state(true); \
		return r; \
	} \
	NNCPP__DEFINE_SUBVIEW_SECTION_DIRECT(name)
/* \endcond */


namespace nnc
{
	class ncch_section final : public c_read_stream<nnc_ncch_section_stream>
	{ public: using c_read_stream::c_read_stream; };

	class ncch_exefs;
	class ncch final
	{
	public:
		ncch() { }
#if NNCPP_ALLOW_IGNORE_ERRORS
		ncch(read_stream_like& rs, keyset::param kset = keyset::default_value(), seeddb::param sdb = seeddb::default_value()) { this->read(rs, kset, sdb); }
#endif
		result read(read_stream_like& rs, keyset::param kset = keyset::default_value(), seeddb::param sdb = seeddb::default_value())
		{
			result res = (result) nnc_read_ncch_header(rs.as_rstream(), &this->hdr);
			if(res == result::ok)
				res = (result) nnc_fill_keypair(&this->kpair.pair, kset, sdb, &this->hdr);
			if(res == result::ok)
				this->rsl = &rs;
			return res;
		}

		enum class crypto_method {
			vinitial = NNC_CRYPT_INITIAL,
			v700     = NNC_CRYPT_700,
			v930     = NNC_CRYPT_930,
			v960     = NNC_CRYPT_960,
		};

		enum class cplatform {
			old3ds = NNC_NCCH_O3DS,
			new3ds = NNC_NCCH_N3DS,
		};

		enum class ctype {
			data          = NNC_NCCH_DATA,
			exe           = NNC_NCCH_EXE,
			system_update = NNC_NCCH_SYS_UPDATE,
			manual        = NNC_NCCH_MANUAL,
			trial         = NNC_NCCH_TRIAL,
		};

		enum class flag {
			fixed_key = NNC_NCCH_FIXED_KEY,
			no_romfs  = NNC_NCCH_NO_ROMFS,
			no_crypto = NNC_NCCH_NO_CRYPTO,
			uses_seed = NNC_NCCH_USES_SEED,
		};

		NNCPP__DEFINE_U128_GETTER_SETTER(keyy, this->hdr.keyy)
		NNCPP__DEFINE_GETTER_SETTER(content_size, this->hdr.content_size, u32) ///< Media units!
		NNCPP__DEFINE_GETTER_SETTER(partition_id, this->hdr.partition_id, u32)
		NNCPP__DEFINE_ARRAY_GETTER_SETTER(maker_code, this->hdr.maker_code, byte_array<3>) ///< ASCII text + NULL terminator
		NNCPP__DEFINE_GETTER_SETTER(version, this->hdr.version, u16)
		NNCPP__DEFINE_ARRAY_GETTER_SETTER(seed_hash, this->hdr.seed_hash, byte_array<4>)
		NNCPP__DEFINE_GETTER_SETTER(title_id, this->hdr.title_id, nnc::title_id)
		NNCPP__DEFINE_ARRAY_GETTER_SETTER(logo_hash, this->hdr.logo_hash, sha256)
		NNCPP__DEFINE_ARRAY_GETTER_SETTER(product_code, this->hdr.product_code, byte_array<17>) ///< ASCII text + NULL terminator
		NNCPP__DEFINE_ARRAY_GETTER_SETTER(exheader_hash, this->hdr.exheader_hash, sha256)
		NNCPP__DEFINE_GETTER_SETTER(exheader_size, this->hdr.exheader_size, u32) ///< Bytes!
		NNCPP__DEFINE_ENUM_GETTER_SETTER(crypt_method, this->hdr.crypt_method, u8, crypto_method)
		NNCPP__DEFINE_ENUM_GETTER_SETTER(platform, this->hdr.platform, u8, cplatform)
		NNCPP__DEFINE_ENUM_GETTER_SETTER(type, this->hdr.type, u8, ctype)
		NNCPP__DEFINE_GETTER_SETTER(content_unit, this->hdr.content_unit, u32)
		NNCPP__DEFINE_GETTER_SETTER(flags, this->hdr.flags, bitfield<u8, flag>)
		NNCPP__DEFINE_GETTER_SETTER(plain_offset, this->hdr.plain_offset, u32) ///< Media units!
		NNCPP__DEFINE_GETTER_SETTER(plain_size, this->hdr.plain_size, u32) ///< Media units!
		NNCPP__DEFINE_GETTER_SETTER(logo_offset, this->hdr.logo_offset, u32) ///< Media units!
		NNCPP__DEFINE_GETTER_SETTER(logo_size, this->hdr.logo_size, u32) ///< Media units!
		NNCPP__DEFINE_GETTER_SETTER(exefs_offset, this->hdr.exefs_offset, u32) ///< Media units!
		NNCPP__DEFINE_GETTER_SETTER(exefs_size, this->hdr.exefs_size, u32) ///< Media units!
		NNCPP__DEFINE_GETTER_SETTER(exefs_hash_size, this->hdr.exefs_hash_size, u32) ///< Media units!
		NNCPP__DEFINE_GETTER_SETTER(romfs_offset, this->hdr.romfs_offset, u32) ///< Media units!
		NNCPP__DEFINE_GETTER_SETTER(romfs_size, this->hdr.romfs_size, u32) ///< Media units!
		NNCPP__DEFINE_GETTER_SETTER(romfs_hash_size, this->hdr.romfs_hash_size, u32) ///< Media units!
		NNCPP__DEFINE_ARRAY_GETTER_SETTER(exefs_hash, this->hdr.exefs_hash, sha256)
		NNCPP__DEFINE_ARRAY_GETTER_SETTER(romfs_hash, this->hdr.romfs_hash, sha256)

		NNCPP__DEFINE_NCCH_SECTION(romfs, nnc_ncch_section_romfs)
		NNCPP__DEFINE_NCCH_SECTION(exefs_header, nnc_ncch_section_exefs_header)
		NNCPP__DEFINE_NCCH_SECTION(exheader, nnc_ncch_section_exheader)
		NNCPP__DEFINE_SUBVIEW_SECTION(plain, nnc_ncch_section_plain)
		NNCPP__DEFINE_SUBVIEW_SECTION(logo, nnc_ncch_section_logo)

		result romfs_section(romfs& out_romfs)
		{
			ncch_section stream;
			result res = this->romfs_section(stream);
			if(res != result::ok) return res;
			return out_romfs.read(stream);
		}

		/* because we don't know about ncch_exefs yet.... */
		template <typename T = ncch_exefs>
		result exefs_section(T& out_exefs)
		{
			return out_exefs.read(*this);
		}

	private:
		friend class ncch_exefs;

		read_stream_like *rsl = nullptr;
		nnc_ncch_header hdr;
		keypair kpair;

	};

	/* this is quite tricky, since ncch may use crypto
	 * it makes more sense for me for this exefs specialisation to be here */
	class ncch_exefs final : public detail::exefs_base<ncch_section>
	{
	public:
		ncch_exefs() { }

#if NNCPP_ALLOW_IGNORE_ERRORS
		ncch_exefs(read_stream_like& rs, nnc::ncch& ncch) { this->read(rs, ncch); }
		ncch_exefs(nnc::ncch& ncch) { this->read(ncch); }
#endif

		result read(nnc::ncch& ncch)
		{
			nnc::ncch_section section;
			/* we only need this section to read the headers; the rest will be encrypted */
			result res = ncch.exefs_header_section(section);
			return res == result::ok ? this->read(section, ncch) : res;
		}

		result read(read_stream_like& rs, nnc::ncch& ncch)
		{
			result res = (result) nnc_read_exefs_header(rs.as_rstream(), (nnc_exefs_file_header *) this->fhs, nullptr);
			if(res == result::ok)
				this->ncch = &ncch;
			return res;
		}

	protected:
		result open_file_impl(file_header& hdr, ncch_section& out) override
		{
			result res = (result) nnc_ncch_exefs_subview(&this->ncch->hdr, this->ncch->rsl->as_rstream(), &this->ncch->kpair.pair, out.csubstream(), hdr);
			if(res == result::ok)
				out.set_open_state(true);
			return res;
		}

	private:
		nnc::ncch *ncch;

	};
}

#endif

