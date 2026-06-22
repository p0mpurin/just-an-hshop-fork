
#ifndef inc_nnc_nncpp_utf_hh
#define inc_nnc_nncpp_utf_hh

#include <nncpp/base.hh>
#include <nnc/utf.h>
#include <string>


namespace nnc
{
	class string final
	{
	public:
		~string()
		{
			if(this->flags & string::owning)
				free(this->ptr);
		}

		void give()
		{
			if(!(this->flags & string::is_const))
				this->flags |= string::owning;
		}

		std::string as_utf8() const
		{
			if(this->flags & string::is_utf16)
			{
				int alloced = this->len * 2;
				u8 *cret = (u8 *) malloc(alloced + 1);
				size_t alen = nnc_utf16_to_utf8(cret, alloced, this->utf16, len);
				std::string ret { (char *) cret, alen };
				free(cret);
				return ret;
			}
			else
				return std::string { (char *) this->utf8, this->len };
		}

		std::u16string as_utf16() const
		{
			if(this->flags & string::is_utf16)
				return std::u16string { (char16_t *) this->utf16, this->len };
			else
			{
				int alloced = len * 2;
				u16 *cret = (u16 *) malloc(alloced + 1);
				size_t alen = nnc_utf8_to_utf16(cret, alloced, this->utf8, len);
				std::u16string ret { (char16_t *) cret, alen };
				free(utf8);
				return ret;
			}
		}

		size_t size() const { return this->len; }

		friend std::ostream& operator << (std::ostream& os, const string& s)
		{
			os << s.as_utf8();
			return os;
		}

		static string from_utf16(const nnc::u16 *ptr, size_t len) { return string { (nnc::u16 *) ptr, len, string::is_const }; }
		static string from_utf8(const nnc::u8 *ptr, size_t len) { return string { (nnc::u8 *) ptr, len, string::is_const }; }
		static string from_utf16(nnc::u16 *ptr, size_t len) { return string { ptr, len }; }
		static string from_utf8(nnc::u8 *ptr, size_t len) { return string { ptr, len }; }

	private:
		/* dangerous constructors */
		string(nnc::u8 *ptr, size_t len, int eflag = 0)
			: len(len), utf8(ptr), flags(eflag) { }
		string(nnc::u16 *ptr, size_t len, int eflag = 0)
			: len(len), utf16(ptr), flags(string::is_utf16 | eflag) { }

		enum string_flags {
			is_utf16 = 1,
			owning   = 2,
			is_const = 4,
		};
		size_t len;
		union {
			void *ptr;
			u16  *utf16;
			u8   *utf8;
		};
		int flags;
	};
}

#endif

