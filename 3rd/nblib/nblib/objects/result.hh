#ifndef nblib_objects_result_hh
#define nblib_objects_result_hh

#include <string>

#include <nblib/nb.hh>

namespace nb
{
	struct ResultStatusCode
	{
	public:
		enum Namespace : u16
		{
			Title        = 1,
			Category     = 2,
			Subcategory  = 3,
			Token        = 4,
			Log          = 5,
			User         = 6,
			Index        = 7,
			WikiPage     = 8,
			Announcement = 9,
			Site         = 10,
			ThsRelease   = 11,
			Internal     = 12
		};

		enum Reason : u16
		{
			Success             = 0,
			Unauthorized        = 1,
			NotFound            = 2,
			InvalidArgument     = 3,
			NoArgumentsProvided = 4,
			ExceptionOccurred   = 5,
			AlreadyExists       = 6,
			InvalidOperation    = 7,
		};

		u32 raw;

		constexpr ResultStatusCode() : raw(0) {}
		constexpr ResultStatusCode(u32 raw) : raw(raw) {}

		Reason reason() const { return (Reason)(this->raw & 0xFFFF); }
		Namespace s_namespace() const { return (Namespace)((this->raw >> 16) & 0xFFFF); }
	};

	static_assert(sizeof(StatusCode) == 4, "Invalid nb::StatusCode size");

	template<typename TString>
	struct NbResult
	{
		ResultStatusCode code;
		TString message;
	};

	using NbRawResult = NbResult<nb::BlobPtr>;

	class Result : public NbResult<std::string>
	{
	public:
		static constexpr const char *magic = "RSLT";

		nb::StatusCode deserialize(u8 *header, u32 header_size, u8 *blob, u32 blob_size)
		{
			if (header_size < sizeof(NbRawResult))
				return nb::StatusCode::INPUT_DATA_TOO_SHORT;

			NbResult<nb::BlobPtr> *rhdr = reinterpret_cast<NbRawResult *>(header);

			nb::ldr(this->code, rhdr->code);

			if (nb::ldr(rhdr->message)) this->message = std::string(&(reinterpret_cast<char *>(blob))[nb::ldr(rhdr->message)]);

			return nb::StatusCode::SUCCESS;
		}
	};
};

#endif
