
#ifndef inc_nnc_nncpp_crypto_hh
#define inc_nnc_nncpp_crypto_hh

#include <nncpp/base.hh>
#include <nncpp/stream.hh>
#include <nncpp/u128.hh>
#include <nnc/crypto.h>
#include <cstring>

/* \cond INTERNAL */
#if NNCPP_ALLOW_IGNORE_ERRORS
	#define NNCPP__DEFINE_SHA_WRAPPER_CTORS(class_name) \
			class_name(read_stream_like& stream) { this->hash(stream); } \
			class_name(read_stream_like& stream, size_t nbytes) { this->hash(stream, nbytes); }
#else
	#define NNCPP__DEFINE_SHA_WRAPPER_CTORS(class_name)
#endif
/* \endcond */


namespace nnc
{
	template <size_t S>
	class sha_hash : public byte_array<S>
	{
	public:
		using byte_array<S>::byte_array;

		result hash(read_stream_like& stream)
		{
			return this->hash_impl(stream, stream.as_rstream()->funcs->size(stream.as_rstream()));
		}

		result hash(read_stream_like& stream, size_t nbytes)
		{
			return this->hash_impl(stream, nbytes);
		}

		friend bool operator == (const sha_hash& a, const sha_hash& b)
		{
			/* comparing sha1 and sha256 should always return false */
			if(a.size() != b.size())
				return false;
			return std::memcmp(a.const_data(), b.const_data(), a.size()) == 0;
		}

		friend std::ostream& operator << (std::ostream& os, const sha_hash& hash)
		{
			char buf[3];
			for(size_t i = 0; i < hash.size(); ++i)
			{
				sprintf(buf, "%02X", hash.data_store[i]);
				os << buf;
			}
			return os;
		}

	protected:
		virtual result hash_impl(read_stream_like& stream, size_t nbytes) = 0;

	};

	class sha256 final : public sha_hash<0x20>
	{
	public:
		using sha_hash::sha_hash;
		NNCPP__DEFINE_SHA_WRAPPER_CTORS(sha256)

		result hash_impl(read_stream_like& stream, size_t nbytes) override
		{
			return (result) nnc_crypto_sha256_part(stream.as_rstream(), this->data_store, nbytes);
		}
	};

	class sha1 final : public sha_hash<20>
	{
	public:
		using sha_hash::sha_hash;
		NNCPP__DEFINE_SHA_WRAPPER_CTORS(sha1)

		result hash_impl(read_stream_like& stream, size_t nbytes) override
		{
			return (result) nnc_crypto_sha1_part(stream.as_rstream(), this->data_store, nbytes);
		}
	};

	class keyset final
	{
	public:
		enum type
		{
			retail = NNC_KEYSET_RETAIL,
			development = NNC_KEYSET_DEVELOPMENT,
		};

		using param = nnc_keyset *;

		keyset(type ktype)
		{
			/* this is what the NNC_KEYSET_INIT macro does */
			this->kset.flags = 0;
			nnc_keyset_default(&this->kset, ktype);
		}

		operator nnc_keyset* () { return &this->kset; }

		NNCPP__DEFINE_U128_GETTER_SETTER(ncch0, this->kset.kx_ncch0)
		NNCPP__DEFINE_U128_GETTER_SETTER(ncch1, this->kset.kx_ncch1)
		NNCPP__DEFINE_U128_GETTER_SETTER(ncchA, this->kset.kx_ncchA)
		NNCPP__DEFINE_U128_GETTER_SETTER(ncchB, this->kset.kx_ncchB)
		NNCPP__DEFINE_U128_GETTER_SETTER(comy0, this->kset.ky_comy0)
		NNCPP__DEFINE_U128_GETTER_SETTER(comy1, this->kset.ky_comy1)
		NNCPP__DEFINE_U128_GETTER_SETTER(comy2, this->kset.ky_comy2)
		NNCPP__DEFINE_U128_GETTER_SETTER(comy3, this->kset.ky_comy3)
		NNCPP__DEFINE_U128_GETTER_SETTER(comy4, this->kset.ky_comy4)
		NNCPP__DEFINE_U128_GETTER_SETTER(comy5, this->kset.ky_comy5)

		void use_as_default()
		{
			nnc_set_default_keyset(&this->kset);
		}

		static param default_value() { return nnc_get_default_keyset(); }

	private:
		nnc_keyset kset;

	};

	class keypair final
	{
		friend class ncch_exefs;
		friend class ncch;
		nnc_keypair pair;
	};

	using seed = byte_array<NNC_SEED_SIZE>;

	class seeddb final
	{
	public:
		using param = nnc_seeddb *;

		enum class init_flag {
			scan        = 1,
			use_default = 2,
		};

		seeddb(init_flag flags = (init_flag) 0)
		{
			int rflags = (int) flags;
			/* this is allowed even without NNCPP_ALLOW_IGNORE_ERRORS because it doesn't matter if scanning fails */
			if(rflags & (int) init_flag::scan) (void) this->scan();
			else                               { this->seeds.size = 0; this->seeds.entries = nullptr; }
			if(rflags & (int) init_flag::use_default)
				this->use_as_default();
		}

#if NNCPP_ALLOW_IGNORE_ERRORS
		seeddb(read_stream_like& rs)
		{
			this->read(rs);
		}
#endif

		~seeddb()
		{
			nnc_free_seeddb(&this->seeds);
		}

		operator nnc_seeddb* () { return &this->seeds; }

		result read(read_stream_like& rs)
		{
			return (result) nnc_seeds_seeddb(rs.as_rstream(), &this->seeds);
		}

		result scan()
		{
			return (result) nnc_scan_seeddb(&this->seeds);
		}

		bool find_seed(seed& res, nnc::title_id tid)
		{
			nnc::u8 *cres = nnc_get_seed(&this->seeds, tid);
			if(!cres) return false;
			res.copy(cres);
			return true;
		}

		void use_as_default()
		{
			nnc_set_default_seeddb(&this->seeds);
		}

		static param default_value() { return nnc_get_default_seeddb(); }

	private:
		nnc_seeddb seeds;

	};
	NNCPP__DEFINE_ENUM_BITWISE_OPERATORS(seeddb::init_flag)

	/* aditional crypto methods */

	class aes_ctr final : public c_read_stream<nnc_aes_ctr>
	{
	public:
		using c_read_stream::c_read_stream;

#if NNCPP_ALLOW_IGNORE_ERRORS
		aes_ctr(read_stream_like& child, u128& key, byte_array<0x10>& iv)
		{
			this->open(child, key, iv);
		}
#endif

		result open(read_stream_like& child, u128& key, byte_array<0x10>& iv)
		{
			result ret = (result) nnc_aes_ctr_open(&this->stream, child.as_rstream(), key, iv.data());
			if(ret == result::ok)
				this->set_open_state(true);
			return ret;
		}
	};

	class aes_cbc final : public c_read_stream<nnc_aes_cbc>
	{
	public:
		using c_read_stream::c_read_stream;

#if NNCPP_ALLOW_IGNORE_ERRORS
		aes_cbc(read_stream_like& child, byte_array<0x10>& key, byte_array<0x10>& iv)
		{
			this->open(child, key, iv);
		}
#endif

		result open(read_stream_like& child, byte_array<0x10>& key, byte_array<0x10>& iv)
		{
			result ret = (result) nnc_aes_cbc_open(&this->stream, child.as_rstream(), key.data(), iv.data());
			if(ret == result::ok)
				this->set_open_state(true);
			return ret;
		}
	};

	/* additional misc crypto methods */
}

#endif

