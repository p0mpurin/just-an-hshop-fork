
#ifndef inc_nnc_nncpp_u128_hh
#define inc_nnc_nncpp_u128_hh

#include <nncpp/base.hh>
#include <nnc/u128.h>
#include <string>


namespace nnc
{
	class u128 final
	{
	public:
		u128(nnc_u128 ni) : i(ni) { }
		u128(u64 ni) : i(NNC_PROMOTE128(ni)) { }
		u128() : i(NNC_PROMOTE128(0)) { }

		static u128 from_string(const char *str)
		{
			return u128 { nnc_u128_from_hex(str) };
		}

		static u128 from_string(const std::string& str)
		{
			return u128 { nnc_u128_from_hex(str.c_str()) };
		}

		static u128 from_bytes(u8 bytes[0x10])
		{
			return u128 { nnc_u128_import_be(bytes) };
		}

		static u128 from_bytes(byte_array<0x10> bytes)
		{
			return u128 { nnc_u128_import_be(bytes.data()) };
		}

		operator u8  () { return this->i.lo & 0xFF; }
		operator u16 () { return this->i.lo & 0xFFFF; }
		operator u32 () { return this->i.lo & 0xFFFFFFFF; }
		operator u64 () { return this->i.lo; }
		operator nnc_u128* () { return &this->i; }

		friend u128 operator + (u128& lhs, u128& rhs)
		{
			nnc_u128 ret = *lhs;
			nnc_u128_add(&ret, rhs);
			return ret;
		}

		friend void operator += (u128& lhs, u128& rhs)
		{
			nnc_u128_add(lhs, rhs);
		}

		friend u128 operator ^ (u128& lhs, u128& rhs)
		{
			nnc_u128 ret = *lhs;
			nnc_u128_xor(&ret, rhs);
			return ret;
		}

		friend void operator ^= (u128& lhs, u128& rhs)
		{
			nnc_u128_xor(lhs, rhs);
		}

		void rol(u128& rhs)
		{
			nnc_u128_rol(&this->i, rhs);
		}

		void ror(u128& rhs)
		{
			nnc_u128_ror(&this->i, rhs);
		}

		void as_bytes(byte_array<0x10>& bytes)
		{
			nnc_u128_bytes_be(&this->i, bytes.data());
		}

		nnc_u128 as_c_u128() const
		{
			return this->i;
		}

		friend std::ostream& operator << (std::ostream& os, u128&& rhs)
		{
			char output[33];
			if(rhs.i.hi) sprintf(output, "%" PRIX64 "%" PRIX64, rhs.i.hi, rhs.i.lo);
			else         sprintf(output, "%" PRIX64,                      rhs.i.lo);
			os << output;
			return os;
		}

	private:
		nnc_u128 i;
	};
}

#endif

