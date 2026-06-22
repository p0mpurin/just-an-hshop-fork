
#ifndef inc_nnc_nncpp_smdh_hh
#define inc_nnc_nncpp_smdh_hh

#include <nncpp/stream.hh>
#include <nncpp/base.hh>
#include <nncpp/utf.hh>
#include <nnc/smdh.h>


namespace nnc
{
	class smdh_title_ref final
	{
	public:
		smdh_title_ref(nnc_smdh_title *ref)
			: title(ref) { }

		NNCPP__DEFINE_UTF16_STATIC_SIZE_GETTER_SETTER(short_description, this->title->short_desc, 0x40);
		NNCPP__DEFINE_UTF16_STATIC_SIZE_GETTER_SETTER(long_description, this->title->long_desc, 0x80);
		NNCPP__DEFINE_UTF16_STATIC_SIZE_GETTER_SETTER(publisher, this->title->publisher, 0x40);

	private:
		nnc_smdh_title *title;
	};

	class smdh final
	{
	public:
		enum class ratingflag
		{
			none     = NNC_RATING_NONE,
			all_ages = NNC_RATING_ALL_AGES,
			pending  = NNC_RATING_PENDING,
			active   = NNC_RATING_ACTIVE,
		};
		
		enum class ratingslot
		{
			cero      = NNC_RATING_CERO,
			esrb      = NNC_RATING_ESRB,
			usk       = NNC_RATING_USK,
			pegi_gen  = NNC_RATING_PEGI_GEN,
			pegi_prt  = NNC_RATING_PEGI_PRT,
			pegi_bbfc = NNC_RATING_PEGI_BBFC,
			cob       = NNC_RATING_COB,
			grb       = NNC_RATING_GRB,
			cgsrr     = NNC_RATING_CGSRR,
		};

		class rating : public bitfield<u8, ratingflag>
		{
		public:
			using bitfield<u8, ratingflag>::bitfield;
			u8 minimum_age() { return NNC_RATING_MIN_AGE(this->field); }
			void set_minimum_age(u8 age) { this->field |= age & 0x1f; }

		};

		enum class region
		{
			japan         = NNC_LOCKOUT_JAPAN,
			north_america = NNC_LOCKOUT_NORTH_AMERICA,
			europe        = NNC_LOCKOUT_EUROPE,
			australia     = NNC_LOCKOUT_AUSTRALIA,
			china         = NNC_LOCKOUT_CHINA,
			korea         = NNC_LOCKOUT_KOREA,
			taiwan        = NNC_LOCKOUT_TAIWAN,
			free          = NNC_LOCKOUT_FREE,
		};

		enum class flag
		{
			visible         = NNC_SMDH_FLAG_VISIBLE,
			autoboot        = NNC_SMDH_FLAG_AUTOBOOT,
			allow_3d        = NNC_SMDH_FLAG_ALLOW_3D,
			require_eula    = NNC_SMDH_FLAG_REQUIRE_EULA,
			autosave        = NNC_SMDH_FLAG_AUTOSAVE,
			extended_banner = NNC_SMDH_FLAG_EXTENDED_BANNER,
			rating_required = NNC_SMDH_FLAG_RATING_REQUIRED,
			savedata        = NNC_SMDH_FLAG_SAVEDATA,
			record_usage    = NNC_SMDH_FLAG_RECORD_USAGE,
			disable_backup  = NNC_SMDH_FLAG_DISABLE_BACKUP,
			n3ds_only       = NNC_SMDH_FLAG_N3DS_ONLY,
		};

		enum class lang
		{
			japanese            = NNC_TITLE_JAPANESE,
			english             = NNC_TITLE_ENGLISH,
			french              = NNC_TITLE_FRENCH,
			german              = NNC_TITLE_GERMAN,
			italian             = NNC_TITLE_ITALIAN,
			spanish             = NNC_TITLE_SPANISH,
			simplified_chinese  = NNC_TITLE_SIMPLIFIED_CHINESE,
			korean              = NNC_TITLE_KOREAN,
			dutch               = NNC_TITLE_DUTCH,
			portuguese          = NNC_TITLE_PORTUGUESE,
			russian             = NNC_TITLE_RUSSIAN,
			traditional_chinese = NNC_TITLE_TRADITIONAL_CHINESE,
		};

		smdh() { }

#if NNCPP_ALLOW_IGNORE_ERRORS
		smdh(read_stream_like& rs)
		{
			this->read(rs);
		}
#endif

		result read(read_stream_like& rs)
		{
			return (result) nnc_read_smdh(rs.as_rstream(), &this->csmdh);
		}

		NNCPP__DEFINE_GETTER_SETTER(version, this->csmdh.version, u16);
		NNCPP__DEFINE_GETTER_SETTER(lockout, this->csmdh.lockout, bitfield<u32, region>);
		NNCPP__DEFINE_GETTER_SETTER(match_maker_id, this->csmdh.match_maker_id, u32);
		NNCPP__DEFINE_GETTER_SETTER(match_maker_bit_id, this->csmdh.match_maker_bit_id, u64);
		NNCPP__DEFINE_GETTER_SETTER(flags, this->csmdh.flags, bitfield<u32, flag>);
		NNCPP__DEFINE_GETTER_SETTER(eula_version_minor, this->csmdh.eula_version_minor, u16);
		NNCPP__DEFINE_GETTER_SETTER(eula_version_major, this->csmdh.eula_version_major, u16);
		NNCPP__DEFINE_GETTER_SETTER(optimal_animation_frame, this->csmdh.optimal_animation_frame, f32);
		NNCPP__DEFINE_GETTER_SETTER(cec_id, this->csmdh.cec_id, u32);

		void set_game_rating(ratingslot slot, rating rate) { this->csmdh.game_ratings[(int) slot] = (u8) rate; }
		rating game_rating(ratingslot slot) { return rating { this->csmdh.game_ratings[(int) slot] }; }

		smdh_title_ref title(lang ln)
		{
			return smdh_title_ref { &this->csmdh.titles[(int) ln] };
		}

	private:
		nnc_smdh csmdh;
	};
	NNCPP__DEFINE_ENUM_BITWISE_OPERATORS(smdh::ratingflag)
	NNCPP__DEFINE_ENUM_BITWISE_OPERATORS(smdh::region)
	NNCPP__DEFINE_ENUM_BITWISE_OPERATORS(smdh::flag)
}

#endif

