
#ifndef inc_nnc_nncpp_base_hh
#define inc_nnc_nncpp_base_hh

#if __cplusplus < 201103L
	#error "nncpp requires at least C++11"
#endif

#include <nnc/base.h>
#include <inttypes.h>
#include <iostream>
#include <cstring>


/* \cond INTERNAL */
#ifndef NNCPP__NODISCARD
	#if NNCPP_ALLOW_IGNORE_ERRORS
		#define NNCPP__NODISCARD
	#else
		#define NNCPP__NODISCARD [[nodiscard]]
	#endif
#endif

#define NNCPP__DEFINE_ENUM_BITWISE_OPERATORS(enum_name) \
	enum_name operator | (enum_name A, enum_name B) { return (enum_name) (((u64) A) | ((u64) B)); } \
	enum_name operator & (enum_name A, enum_name B) { return (enum_name) (((u64) A) & ((u64) B)); }
/* the ... for type because templates with 1+ args exist */
#define NNCPP__DEFINE_GETTER_SETTER(cpp_name, c_name, ...) \
	__VA_ARGS__ cpp_name() { return c_name; } \
	void set_##cpp_name(__VA_ARGS__ prop) { c_name = prop; }
#define NNCPP__DEFINE_ENUM_GETTER_SETTER(cpp_name, c_name, orig_type, ...) \
	__VA_ARGS__ cpp_name() { return (__VA_ARGS__) c_name; } \
	void set_##cpp_name(__VA_ARGS__ prop) { c_name = (orig_type) prop; }
#define NNCPP__DEFINE_ENUM_GETTER(cpp_name, c_name, orig_type, ...) \
	__VA_ARGS__ cpp_name() { return (__VA_ARGS__) c_name; }
#define NNCPP__DEFINE_ARRAY_GETTER_SETTER(cpp_name, c_name, ...) \
	__VA_ARGS__ cpp_name() { return c_name; } \
	void set_##cpp_name(const __VA_ARGS__& prop) { std::memcpy(c_name, prop.const_data(), prop.size()); }
#define NNCPP__DEFINE_UTF16_STATIC_SIZE_GETTER_SETTER(cpp_name, c_name, len) \
	nnc::string cpp_name() { return nnc::string::from_utf16(c_name, len); } \
	void set_##cpp_name(const nnc::string& prop) { \
		if(prop.size() <= (unsigned) len) { \
			std::u16string utf16 = prop.as_utf16(); \
			std::memcpy(c_name, utf16.c_str(), (utf16.size()) * sizeof(nnc::u16)); \
			std::memset(c_name + ((utf16.size()) * sizeof(nnc::u16)), 0x00, ((len) * sizeof(nnc::u16)) - ((utf16.size()) * sizeof(nnc::u16))); \
	} /* TODO: what to do in the else case? */ }
#define NNCPP__DEFINE_UTF16_VAR_SIZE_GETTER(cpp_name, c_name, len_field) \
	nnc::string cpp_name() { return nnc::string::from_utf16(c_name, len_field); }
#define NNCPP__DEFINE_U128_GETTER_SETTER(cpp_name, c_name) \
	u128 cpp_name() { return c_name; } \
	void set_##cpp_name(const nnc::u128& prop) { c_name = prop.as_c_u128(); }
#define NNCPP__MUST_WRAP(cpp_name, c_name) \
	static_assert(sizeof(cpp_name) == sizeof(c_name), #c_name " is not the only and/or first field of " #cpp_name "!");

#define NNCPP_PRIVATE(sym) _nncpp$PRIV$##sym
#define NNCPP_FOREACH(value_name, iterator) \
	decltype(iterator) NNCPP_PRIVATE(it) = (iterator); \
	decltype(iterator.NNCPP_PRIVATE(type_for_decltype())) value_name; \
	while(NNCPP_PRIVATE(it).next(value_name))
/* \endcond */


namespace nnc
{
	using  u8 = nnc_u8;
	using u16 = nnc_u16;
	using u32 = nnc_u32;
	using u64 = nnc_u64;

	using  i8 = nnc_i8;
	using i16 = nnc_i16;
	using i32 = nnc_i32;
	using i64 = nnc_i64;

	using f32 = nnc_f32;
	using f64 = nnc_f64;

	constexpr int media_unit = NNC_MEDIA_UNIT;

	template <typename T>
	constexpr T media_unit_to_bytes(T mus)
	{
		return NNC_MU_TO_BYTE(mus);
	}

	using cresult = nnc_result;
	enum class NNCPP__NODISCARD result : nnc_result {
		ok             = NNC_R_OK,
		fail_open      = NNC_R_FAIL_OPEN,
		seek_range     = NNC_R_SEEK_RANGE,
		fail_read      = NNC_R_FAIL_READ,
		too_small      = NNC_R_TOO_SMALL,
		too_large      = NNC_R_TOO_LARGE,
		invalid_sig    = NNC_R_INVALID_SIG,
		corrupt        = NNC_R_CORRUPT,
		nomem          = NNC_R_NOMEM,
		not_found      = NNC_R_NOT_FOUND,
		not_a_file     = NNC_R_NOT_A_FILE,
		key_not_found  = NNC_R_KEY_NOT_FOUND,
		mismatch       = NNC_R_MISMATCH,
		seed_not_found = NNC_R_SEED_NOT_FOUND,
		unsupported    = NNC_R_UNSUPPORTED,
		inval          = NNC_R_INVAL,
		bad_align      = NNC_R_BAD_ALIGN,
		bad_sig	       = NNC_R_BAD_SIG,
		cert_not_found = NNC_R_CERT_NOT_FOUND,
		invalid_cert   = NNC_R_INVALID_CERT,
		fail_write     = NNC_R_FAIL_WRITE,
		not_open       = NNC_R_NOT_OPEN,
		open           = NNC_R_OPEN,
	};
	inline void parse_version(u16 version, u8& major, u8& minor, u8& patch)
	{
		nnc_parse_version(version, &major, &minor, &patch);
	}

	inline const char *strerror(result res)
	{
		return nnc_strerror((cresult) res);
	}

	std::ostream& operator << (std::ostream& os, result res)
	{
		os << strerror(res);
		return os;
	}

	inline const char *strerror(cresult res)
	{
		return nnc_strerror(res);
	}

	template <typename TUInt, typename TBitFieldEnum>
	class bitfield
	{
	private:
		using self_ref = bitfield<TUInt, TBitFieldEnum>&;

	public:
		bitfield(TUInt nfield) : field(nfield) { }
		bitfield() : field(0) { }

		operator TUInt () { return this->field; }
		TUInt& raw() { return this->field; }
		self_ref set(TBitFieldEnum bits) { this->field |= (TUInt) bits; return *this; }
		self_ref unset(TBitFieldEnum bits) { this->field &= ~((TUInt) bits); return *this; }
		self_ref clear() { this->field = 0; return *this; }
		bool all_set(TBitFieldEnum bits) { return (this->field & ((TUInt) bits)) == bits; }
		bool any_set(TBitFieldEnum bits) { return !!(this->field & ((TUInt) bits)); }

	protected:
		TUInt field;

	};

	template <typename T>
	class span
	{
	public:
		virtual ~span() { }

		template <typename U = T> const U *const_data() const { return (U *) this->get_pointer(); }
		template <typename U = T> U *data() { return (U *) this->get_pointer(); }

		size_t size() const { return this->get_length(); }

	protected:
		virtual size_t get_length() const = 0;
		virtual T *get_pointer() const = 0;

	};

	template <size_t S>
	class byte_array : public span<u8>
	{
	public:
		static constexpr size_t byte_size = S;

		byte_array() { }
		byte_array(const void *data, size_t size = S)
		{
			this->copy(data, size);
		}

		void copy(const void *data, size_t size = S)
		{
			std::memcpy(this->data_store, data, size);
		}

		u8 operator [] (size_t i) { return this->data_store[i]; }

	protected:
		u8 data_store[S];

		constexpr size_t get_length() const override { return S; }
		u8 *get_pointer() const override { return (u8 *) this->data_store; }

	};


	template <typename T>
	class ref_array final : public span<T>
	{
	public:
		ref_array(T *ptr, size_t len) : ptr(ptr), len(len) { }
		ref_array() : ptr(nullptr), len(0) { }

	protected:
		size_t get_length() const override { return this->len; }
		T *get_pointer() const override { return this->ptr; }

	private:
		T *ptr;
		size_t len;

	};

	template <typename T>
	class dynamic_array : public span<T>
	{
	public:
		~dynamic_array() { free(this->ptr); }
		dynamic_array() : ptr(nullptr), len(0) { }

		bool allocate(size_t n)
		{
			this->ptr = (T *) malloc(n * sizeof(T));
			this->len = n;
			return this->ptr != NULL;
		}

	protected:
		size_t get_length() const override { return this->len; }
		T *get_pointer() const override { return this->ptr; }

	private:
		T *ptr;
		size_t len;

	};

	namespace tid
	{
		enum class category {
			normal    = NNC_TID_CAT_NORMAL,
			dlp_child = NNC_TID_CAT_DEMO,
			demo      = NNC_TID_CAT_DEMO,
			aoc       = NNC_TID_CAT_AOC,
			no_exe    = NNC_TID_CAT_NO_EXE,
			update    = NNC_TID_CAT_UPDATE,
			system    = NNC_TID_CAT_SYSTEM,
			not_mount = NNC_TID_CAT_NOT_MOUNT,
			dlc       = NNC_TID_CAT_DLC,
			twl       = NNC_TID_CAT_TWL,
		};
		NNCPP__DEFINE_ENUM_BITWISE_OPERATORS(category)
	}

	class title_id final
	{
	public:
		title_id(u64 ntid) : tid(ntid) { }
		title_id() : tid(NNC_BASE_TID) { }

		operator u64 () { return this->tid; }
		u64 value() { return this->tid; }

		tid::category category()  { return (tid::category) nnc_tid_category(this->tid); }
		u32           unique_id() { return nnc_tid_unique_id(this->tid); }
		u8            variation() { return nnc_tid_variation(this->tid); }

		void set_category(tid::category cat) { nnc_tid_set_category(&this->tid, (u16) cat); }
		void set_unique_id(u32 unique_id)    { nnc_tid_set_unique_id(&this->tid, unique_id); }
		void set_variation(u8 variation)     { nnc_tid_set_variation(&this->tid, variation); }

		std::string to_string()
		{
			char tid[17];
			sprintf(tid, "%016" PRIX64, this->tid);
			return std::string(tid, 16);
		}

	private:
		u64 tid;

	};

	template <typename TUnder>
	class iterator
	{
	public:
		using UnderlayingType = TUnder;

		UnderlayingType NNCPP_PRIVATE(type_for_decltype());

		virtual bool next(UnderlayingType& next_val) = 0;

	};
}

#endif

