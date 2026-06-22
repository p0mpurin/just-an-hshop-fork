#ifndef nblib_objects_ths_log_hh
#define nblib_objects_ths_log_hh

#include <nblib/nb.hh>

namespace nb
{
	struct NbThsLogResult
	{
		u32 id;
		u64 duration;
	};

	class ThsLogResult : public NbThsLogResult
	{
	public:
		static constexpr const char *magic = "HLOG";

		nb::StatusCode deserialize(u8 *header, u32 header_size, u8 *blob, u32 blob_size)
		{
			if (header_size < sizeof(NbThsLogResult))
				return nb::StatusCode::INPUT_DATA_TOO_SHORT;

			NbThsLogResult *rhdr = reinterpret_cast<NbThsLogResult *>(header);

			nb::ldr(this->id, rhdr->id);
			nb::ldr(this->duration, rhdr->duration);

			return nb::StatusCode::SUCCESS;
		};
	};
};
#endif
