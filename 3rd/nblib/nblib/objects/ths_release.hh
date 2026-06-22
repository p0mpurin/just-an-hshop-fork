#ifndef nblib_objects_ths_release_hh
#define nblib_objects_ths_release_hh

#include <nblib/nb.hh>
#include <string>

namespace nb
{
	template <typename TString>
	struct NbThsRelease
	{
		u64 added;
		TString version;
		TString versiondesc;
		TString changelog;
		TString dlUrl;
		TString sourceUrl;
		TString tdsxUrl;
	};

	class ThsRelease : public NbThsRelease<std::string>
	{
	public:
		static constexpr const char *magic = "3HSR";

		nb::StatusCode deserialize(u8 *header, u32 header_size, u8 *blob, u32 blob_size)
		{
			if (header_size < sizeof(NbThsRelease<nb::BlobPtr>))
				return nb::StatusCode::INPUT_DATA_TOO_SHORT;

			NbThsRelease<nb::BlobPtr> *rhdr = reinterpret_cast<NbThsRelease<nb::BlobPtr> *>(header);

			nb::ldr(this->added, rhdr->added);

			char *strdata = reinterpret_cast<char *>(blob);

			this->version = std::string(&strdata[nb::ldr(rhdr->version)]);
			this->versiondesc = std::string(&strdata[nb::ldr(rhdr->versiondesc)]);
			this->changelog = std::string(&strdata[nb::ldr(rhdr->changelog)]);
			this->dlUrl = std::string(&strdata[nb::ldr(rhdr->dlUrl)]);
			this->sourceUrl = std::string(&strdata[nb::ldr(rhdr->sourceUrl)]);
			this->tdsxUrl = std::string(&strdata[nb::ldr(rhdr->tdsxUrl)]);

			return nb::StatusCode::SUCCESS;
		}
	};
};

#endif
