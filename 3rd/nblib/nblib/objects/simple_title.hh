#ifndef nblib_objects_simple_title_hh
#define nblib_objects_simple_title_hh

#include <nblib/nb.hh>
#include <string>

namespace nb
{
	template <typename TString, typename TRawArray, typename TTitleID>
	struct NbSimpleTitle
	{
		TTitleID tid;
		u64 size;
		u64 flags;
		u64 dlCount;
		u32 id;
		TString name;
		TString alt; /* legacy! */
		TString prod;
		u16 version;
		u8 contentType;
		u8 cat;
		u8 subcat;
		TRawArray alt_names;
		u32 preferred_alt_idx;
	};

	using NbRawSimpleTitle = NbSimpleTitle<nb::BlobPtr, nb::RawArrayPtr, u64>;

	template <typename TTitleID>
	class SimpleTitle : public NbSimpleTitle<std::string, std::vector<std::string>, TTitleID>
	{
	public:
		std::vector<std::string> alt_names;

		nb::StatusCode deserialize(u8 *header, u32 header_size, u8 *blob, u32 blob_size)
		{
			if (header_size < sizeof(NbRawSimpleTitle))
				return nb::StatusCode::INPUT_DATA_TOO_SHORT;

			NbRawSimpleTitle *sthdr = reinterpret_cast<NbRawSimpleTitle *>(header);

			memcpy(&this->tid, &sthdr->tid, sizeof(u64));
			nb::ldr(this->size, sthdr->size);
			nb::ldr(this->flags, sthdr->flags);
			nb::ldr(this->dlCount, sthdr->dlCount);
			nb::ldr(this->id, sthdr->id);
			nb::ldr(this->version, sthdr->version);
			nb::ldr(this->contentType, sthdr->contentType);
			nb::ldr(this->cat, sthdr->cat);
			nb::ldr(this->subcat, sthdr->subcat);
			nb::ldr(this->preferred_alt_idx, sthdr->preferred_alt_idx);

			char *strdata = reinterpret_cast<char *>(blob);

			this->name = std::string(&strdata[nb::ldr(sthdr->name)]);
			this->prod = std::string(&strdata[nb::ldr(sthdr->prod)]);
			if (nb::ldr(sthdr->alt)) this->alt = std::string(&strdata[nb::ldr(sthdr->alt)]);
			if (nb::ldr(sthdr->alt_names))
				return nb::raw_array::parse_inline<std::string>(this->alt_names, nb::raw_helpers::utf8_str, nb::ldr(sthdr->alt_names), blob, blob_size);

			return nb::StatusCode::SUCCESS;
		}

		std::string& preferred_alt_name() {
			return this->alt_names[this->preferred_alt_idx];
		}

		bool has_alt() {
			return this->alt_names.size() != 0;
		}
	};
};

#endif
