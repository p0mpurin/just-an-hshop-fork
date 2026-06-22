#ifndef nblib_nb_raw_array_hh
#define nblib_nb_raw_array_hh

#include <functional>
#include <cstring>
#include <string>
#include <vector>

#include <nblib/nb.hh>

/* vector out, u32 elem count, u8 *buf, u32 bufsize */
template<typename T>
using raw_helper = std::function<nb::StatusCode(std::vector<T> &, u32, u8 *, u32)>;

namespace nb
{
	namespace raw_array
	{
		static constexpr const char magic[4] = { 'N', 'B', 'R', 'A' };

		struct RawArrayHeader
		{
			char magic[4];
			u32 raw_array_header_size;
			u32 element_count;
			u32 blob_size; /* element storage size */

			inline u32 total_size() { return nb::ldr(this->raw_array_header_size) + nb::ldr(this->blob_size); }

			template <typename T>
			inline size_t min_size_as_prim() { return nb::ldr(this->raw_array_header_size) + 4 + nb::align(nb::ldr(this->element_count) * sizeof(T), 4); }
		};

		static nb::StatusCode validate_header(RawArrayHeader *&rhdr, u8 *data, size_t size)
		{
			if (!data || !size)
				return nb::StatusCode::NO_INPUT_DATA;

			if ((uintptr_t) data & 3)
				return nb::StatusCode::UNALIGNED;

			if (memcmp(nb::raw_array::magic, data, sizeof(NbMagic)) != 0)
				return nb::StatusCode::MAGIC_MISMATCH;

			if (size < sizeof(RawArrayHeader))
				return nb::StatusCode::INPUT_DATA_TOO_SHORT;

			RawArrayHeader *_rhdr = reinterpret_cast<RawArrayHeader *>(data);

			if (!(nb::ldr(_rhdr->blob_size) > 4))
				return nb::StatusCode::INPUT_DATA_TOO_SHORT;

			/* header must be aligned so that blob starts aligned; blob shall not be misaligned either */
			if ((nb::ldr(_rhdr->raw_array_header_size) & 3) || (nb::ldr(_rhdr->blob_size) & 3))
				return nb::StatusCode::UNALIGNED;

			if (size < _rhdr->total_size())
				return nb::StatusCode::INPUT_DATA_TOO_SHORT;

			rhdr = _rhdr;
			return nb::StatusCode::SUCCESS;
		}

		template <typename TPrimitive>
		static nb::StatusCode parse_primitive(std::vector<TPrimitive>& out, u8 *raw_array, size_t bufsize)
		{
			RawArrayHeader *rhdr = nullptr;

			nb::StatusCode c = nb::raw_array::validate_header(rhdr, raw_array, bufsize);

			if (c != nb::StatusCode::SUCCESS) return c;

			size_t minsize = rhdr->min_size_as_prim<TPrimitive>();

			/* quick sanity check */
			if (minsize > bufsize)
				return nb::StatusCode::INPUT_DATA_TOO_SHORT;

			out.reserve(nb::ldr(rhdr->element_count));

			TPrimitive *data_prim = reinterpret_cast<TPrimitive *>(&raw_array[nb::ldr(rhdr->raw_array_header_size) + 4]);

			for (u32 i = 0; i < nb::ldr(rhdr->element_count); i++)
				out.push_back(*data_prim++);

			return nb::StatusCode::SUCCESS;
		}

		template <typename TPrimitive>
		static nb::StatusCode parse_primitive_inline(std::vector<TPrimitive>& out, nb::RawArrayPtr array_offset, u8 *blob, size_t blob_size)
		{
			if (!array_offset) return nb::StatusCode::SUCCESS; /* empty array */

			if (array_offset >= blob_size) return nb::StatusCode::INPUT_DATA_TOO_SHORT; /* array offset is out of bounds */

			size_t remain_blob_size = blob_size - array_offset; /* space remaining in given blob starting from the array header to the end */

			u8 *raw_array = &blob[array_offset]; /* points to the array header in the blob */

			return nb::raw_array::parse_primitive(out, raw_array, remain_blob_size); /* parse the array from the blob directly */
		}

		template <typename T>
		static nb::StatusCode parse(std::vector<T>& out, raw_helper<T> helper, u8 *raw_array, size_t bufsize)
		{
			RawArrayHeader *rhdr = nullptr;

			nb::StatusCode c = nb::raw_array::validate_header(rhdr, raw_array, bufsize);

			if (c != nb::StatusCode::SUCCESS) return c;

			return helper(out, nb::ldr(rhdr->element_count), &raw_array[nb::ldr(rhdr->raw_array_header_size) + 4], nb::ldr(rhdr->blob_size) - 4);
		}

		template <typename T>
		static nb::StatusCode parse_inline(std::vector<T>& out, raw_helper<T> helper, nb::RawArrayPtr array_offset, u8 *blob, size_t blob_size)
		{
			if (!array_offset) return nb::StatusCode::SUCCESS; /* empty array */

			if (array_offset >= blob_size) return nb::StatusCode::INPUT_DATA_TOO_SHORT; /* array offset is out of bounds */

			size_t remain_blob_size = blob_size - array_offset; /* space remaining in given blob starting from the array header to the end */

			u8 *raw_array = &blob[array_offset]; /* points to the array header in the blob */

			return nb::raw_array::parse(out, helper, raw_array, remain_blob_size); /* parse the array from the blob directly */
		}
	};

	namespace raw_helpers
	{
		static const nb::StatusCode utf8_str(std::vector<std::string>& out, u32 element_count, u8 *buf, u32 bufsize)
		{
			char *cur = reinterpret_cast<char *>(buf);
			u32 remain = bufsize;
			u32 processed = 0;

			if (!buf || !bufsize) return nb::StatusCode::NO_INPUT_DATA;
			else if (bufsize < 2) return nb::StatusCode::INPUT_DATA_TOO_SHORT; /* the minimum allowed is 1 string of size 1, so char + nullterm */

			/* process at most element_count strings */
			for (u32 i = 0; i < element_count; i++)
			{
				if (remain < 2) /* it is impossible for anything valid to exist in this case */
					break;

				size_t curlen = strnlen(cur, remain);

				if (!curlen || curlen == remain) /* we don't have a string left or it doesn't terminate properly */
					break;

				out.emplace_back(cur, curlen);

				processed++;
				cur += curlen + 1;
				remain -= curlen + 1;
			}

			if (processed != element_count)
				return nb::StatusCode::INPUT_DATA_TOO_SHORT;

			return nb::StatusCode::SUCCESS;
		}
	};
};

#endif
