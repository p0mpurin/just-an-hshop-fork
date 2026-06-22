#ifndef nblib_nb_array_hh
#define nblib_nb_array_hh

#include <nblib/nb.hh>

namespace nb
{
	namespace array
	{
		static constexpr const NbMagic magic = { 'N', 'B', 'A', 'R' };

		struct ArrayHeader
		{
			char magic[4];
			u32  array_header_size;
			u32  object_count;
			u32  element_header_size;
			u32  shared_blob_size;

			inline u32 total_elements_header_size() { return nb::ldr(this->object_count) * nb::ldr(this->element_header_size); }
			inline u32 total_header_size() { return nb::ldr(this->array_header_size) + this->total_elements_header_size(); }
			inline u32 total_size() { return this->total_header_size() + nb::ldr(this->shared_blob_size); }
		};

		static nb::StatusCode validate_header(ArrayHeader *&hdr, u8 *data, size_t size)
		{
			if (!data || !size)
				return nb::StatusCode::NO_INPUT_DATA;

			if ((uintptr_t) data & 3)
				return nb::StatusCode::UNALIGNED;

			if (memcmp(nb::array::magic, data, sizeof(NbMagic)) != 0)
				return nb::StatusCode::MAGIC_MISMATCH;

			if (size < sizeof(ArrayHeader))
				return nb::StatusCode::INPUT_DATA_TOO_SHORT;

			ArrayHeader *_hdr = reinterpret_cast<ArrayHeader *>(data);

			if (nb::ldr(_hdr->element_header_size) & 3)
				return nb::StatusCode::UNALIGNED;

			if (_hdr->total_size() > size)
				return nb::StatusCode::INPUT_DATA_TOO_SHORT;

			hdr = _hdr;

			return nb::StatusCode::SUCCESS;
		}

		template <typename T>
		static nb::StatusCode parse(std::vector<T>& out, u8 *array, size_t bufsize)
		{
			ArrayHeader *hdr = nullptr;

			/* validate array header */
			nb::StatusCode c = nb::array::validate_header(hdr, array, bufsize);

			if (c != nb::StatusCode::SUCCESS) return c;

			out.reserve(nb::ldr(hdr->object_count));

			u8 *array_blob = &array[hdr->total_header_size()];

			c = nb::StatusCode::SUCCESS;

			for (u32 i = 0; i < nb::ldr(hdr->object_count); i++)
			{
				u8 *obj_hdr = &array[nb::ldr(hdr->array_header_size) + i * nb::ldr(hdr->element_header_size)];

				T obj;

				if ((c = obj.deserialize(obj_hdr, nb::ldr(hdr->element_header_size), array_blob, nb::ldr(hdr->shared_blob_size))) != nb::StatusCode::SUCCESS)
					return c;

				out.push_back(obj);
			}

			return nb::StatusCode::SUCCESS;
		}

		template <typename T>
		static nb::StatusCode parse_inline(std::vector<T>& out, nb::ArrayPtr array_offset, u8 *blob, size_t blob_size)
		{
			if (!array_offset) return nb::StatusCode::SUCCESS; /* empty array */

			if (array_offset >= blob_size) return nb::StatusCode::INPUT_DATA_TOO_SHORT; /* array offset is out of bounds */

			size_t remain_blob_size = blob_size - array_offset; /* space remaining in given blob starting from the array header to the end */

			u8 *raw_array = &blob[array_offset]; /* points to the array header in the blob */

			return nb::array::parse<T>(out, raw_array, remain_blob_size); /* parse the array from the blob directly */
		}
	};
};

#endif
