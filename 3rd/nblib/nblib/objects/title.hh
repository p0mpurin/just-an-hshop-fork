#ifndef nblib_objects_title_hh
#define nblib_objects_title_hh

#include <nblib/nb.hh>
#include <string>

namespace nb
{
	template <typename TString, typename TRawArray, typename TTitleID>
	struct NbTitle
	{
		u8 seed[16];
		u64 size;
		TTitleID tid;
		u64 added; /* timestamp */
		u64 updated; /* timestamp */
		u64 dlCount;
		u64 flags;
		u32 id;
		TString name;
		TString alt;
		TString region;
		TString filename;
		TString desc;
		TString prod;
		u16 version;
		u8 contentType; /* piratelegit/legit/stnadard */
		u8 cat;
		u8 subcat;
		bool listed;
		u8 pad[2];
		TRawArray alt_names;
		u32 preferred_alt_idx;
		u8 file_checksum[32];
	};

	using NbTitleRaw = NbTitle<nb::BlobPtr, nb::RawArrayPtr, u64>;

	template <typename TTitleID>
	class Title : public NbTitle<std::string, std::vector<std::string>, TTitleID>
	{
	public:
		static constexpr const char *magic = "TITL";

		void operator=(const Title<TTitleID>& other)
		{
			this->size = other.size;
			this->tid = other.tid;
			this->added = other.added;
			this->updated = other.updated;
			this->dlCount = other.dlCount;
			this->flags = other.flags;
			this->id = other.id;
			this->version = other.version;
			this->contentType = other.contentType;
			this->listed = other.listed;
			this->cat = other.cat;
			this->subcat = other.subcat;
			memcpy(this->seed, other.seed, sizeof(this->seed));
			this->name = other.name;
			this->region = other.region;
			this->filename = other.filename;
			this->prod = other.prod;
			this->desc = other.desc;
			this->alt = other.alt;
			if (other.alt_names.size())
				std::copy(other.alt_names.begin(), other.alt_names.end(), std::back_inserter(this->alt_names));

				this->preferred_alt_idx = other.preferred_alt_idx;
				memcpy(this->file_checksum, other.file_checksum, sizeof(this->file_checksum));
		}

		nb::StatusCode deserialize(u8 *header, u32 header_size, u8 *blob, u32 blob_size)
		{
			if (header_size < sizeof(NbTitleRaw))
				return nb::StatusCode::INPUT_DATA_TOO_SHORT;

			NbTitleRaw *thdr = reinterpret_cast<NbTitleRaw *>(header);

			nb::ldr(this->size, thdr->size);
			memcpy(&this->tid, &thdr->tid, sizeof(u64));
			nb::ldr(this->added, thdr->added);
			nb::ldr(this->updated, thdr->updated);
			nb::ldr(this->dlCount, thdr->dlCount);
			nb::ldr(this->flags, thdr->flags);
			nb::ldr(this->id, thdr->id);
			nb::ldr(this->version, thdr->version);
			nb::ldr(this->contentType, thdr->contentType);
			nb::ldr(this->listed, thdr->listed);
			nb::ldr(this->cat, thdr->cat);
			nb::ldr(this->subcat, thdr->subcat);

			memcpy(this->seed, thdr->seed, 16);

			char *strdata = reinterpret_cast<char *>(blob);

			this->name = std::string(&strdata[nb::ldr(thdr->name)]);
			this->region = std::string(&strdata[nb::ldr(thdr->region)]);
			this->filename = std::string(&strdata[nb::ldr(thdr->filename)]);
			this->prod = std::string(&strdata[nb::ldr(thdr->prod)]);

			if (nb::ldr(thdr->desc)) this->desc = std::string(&strdata[nb::ldr(thdr->desc)]);
			if (nb::ldr(thdr->alt)) this->alt = std::string(&strdata[nb::ldr(thdr->alt)]);
			if (nb::ldr(thdr->alt_names))
			{
				/* Alternate names are optional display metadata. A malformed
				 * list must not make the complete title (and its download)
				 * unusable; retain the legacy single alternate name instead. */
				nb::StatusCode c = nb::raw_array::parse_inline<std::string>(this->alt_names, nb::raw_helpers::utf8_str, nb::ldr(thdr->alt_names), blob, blob_size);
				if (c != nb::StatusCode::SUCCESS)
					this->alt_names.clear();
			}

			nb::ldr(this->preferred_alt_idx, thdr->preferred_alt_idx);
			if (this->preferred_alt_idx >= this->alt_names.size())
				this->preferred_alt_idx = 0;
			memcpy(&this->file_checksum, &thdr->file_checksum, sizeof(this->file_checksum));

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
