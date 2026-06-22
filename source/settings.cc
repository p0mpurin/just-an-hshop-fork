/* This file is part of 3hs
 * Copyright (C) 2021-2025 hShop developer team
 *
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include "settings.hh"

#include <widgets/indicators.hh>

#include <ui/menuselect.hh>
#include <ui/selector.hh>
#include <ui/confirm.hh>
#include <ui/swkbd.hh>
#include <ui/theme.hh>
#include <ui/list.hh>

#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <stdio.h>

#include <algorithm>
#include <cctype>
#include <string>

#include "proxy.hh"
#include "log.hh"

#define SETTINGS_LOCATION "/3ds/3hs/settings"
#define THEMES_DIR        "/3ds/3hs/themes/"
#define BACKGROUNDS_DIR   "/3ds/3hs/backgrounds/"
#define SPECIAL_LIGHT     "special:light"
#define SPECIAL_DARK      "special:dark"
#define SPECIAL_PREFIX    "special:"

enum migrations {
	migration_initial           = 0,
	/* why is the value of 1 missing ...? */
	migration_default_reinstall = 2,
	migration_latency_graph     = 3,
	migration_goto_region       = 4,
	migration_rev_reinstall     = 5,
	migration_enable_mem_log    = 6,

	migration_meta_last,
};
#define LATEST_MIGRATION (migration_meta_last - 1)

static std::vector<ui::Theme> g_avail_themes;
static NewSettings g_nsettings;
static int light_theme_index;
static bool g_loaded = false;
static bool settings_dirtied = false;

/* forward declaration */
static void update_settings_ID(SettingsId id);

NewSettings *get_nsettings()
{ return &g_nsettings; }

static void write_settings_to_file()
{
	mkdir("/3ds", 0777);
	mkdir("/3ds/3hs", 0777); /* ensure these dirs exist */
	FILE *f = fopen(SETTINGS_LOCATION, "w");
	panic_assert(f, "failed to open settings file for writing");

	u8 header[32];
	memcpy(header, "4HSS", 4);
	* (u64 *) &header[0x04] = g_nsettings.flags0;
	header[0x0C] = g_nsettings.lang;
	header[0x0D] = g_nsettings.max_elogs;
	header[0x0E] = g_nsettings.migration;
	memset(&header[0xF], 0, sizeof(u32) * 3 + sizeof(u8) * 3);
	header[0x0F] = g_nsettings.wallpaper_dim;
	* (u16 *) &header[0x1E] = (u16) g_nsettings.theme_path.size();

	panic_assert(fwrite(header, sizeof(header), 1, f) == 1, "failed to write to settings");
	panic_assert(fwrite(g_nsettings.theme_path.data(), g_nsettings.theme_path.size(), 1, f) == 1, "failed to write to settings");
	panic_assert(fwrite(&g_nsettings.proxy_port, sizeof(u16), 1, f) == 1, "failed to write to settings");
#define WRITE_STR(s) do { u16 len = (u16) s.size(); panic_assert(fwrite(&len, sizeof(u16), 1, f) == 1, "failed to write to settings"); if(len) panic_assert(fwrite(s.data(), len, 1, f) == 1, "failed to write to settings"); } while(0)
	if(g_nsettings.proxy_port != 0)
	{
		WRITE_STR(g_nsettings.proxy_host);
		WRITE_STR(g_nsettings.proxy_username);
		WRITE_STR(g_nsettings.proxy_password);
	}
	WRITE_STR(g_nsettings.background_path);
#undef WRITE_STR

	fclose(f);
}

static void write_settings()
{
#ifdef RELEASE
	settings_dirtied = true;
#else
	write_settings_to_file();
#endif
}



void settings_sync()
{
	if(settings_dirtied)
	{
		settings_dirtied = false;
		write_settings_to_file();
	}
}

static void migrate_settings(u8 *buf)
{
	struct Settings
	{
		char magic[4];
		bool isLightMode;
		bool resumeDownloads;
		bool loadFreeSpace;
		bool showBattery;
		bool showNet;
		enum class Timefmt
		{
			good = 24,
			bad = 12
		} timeFormat;
		bool unused0;
		enum class ProgressBarLocation
		{
			top, bottom
		} progloc;
		lang::type language;
		enum class LumaLocaleMode
		{
			disabled, automatic, manual,
		} lumalocalemode;
		bool checkForExtraContent;
		bool warnNoBase;
		u8 maxExtraLogs;
		enum class SortMethod {
			alpha,
			tid,
			size,
			downloads,
			id,
		} defaultSortMethod;
		enum class SortDirection {
			asc,
			desc,
		} defaultSortDirection;
		bool allowLEDChange;
	} *settings = (Settings *) buf;

	g_nsettings.flags0 =
		  (settings->resumeDownloads ? FLAG0_RESUME_DOWNLOADS : 0)
		| (settings->loadFreeSpace ? FLAG0_LOAD_FREE_SPACE : 0)
		| (settings->showBattery ? FLAG0_SHOW_BATTERY : 0)
		| (settings->showNet ? FLAG0_SHOW_NET : 0)
		| (settings->timeFormat == Settings::Timefmt::bad ? FLAG0_BAD_TIME_FORMAT : 0)
		| (settings->progloc == Settings::ProgressBarLocation::top ? FLAG0_PROGBAR_TOP : 0)
		| (((u8) settings->lumalocalemode) << LUMALOCALE_SHIFT)
		| (settings->checkForExtraContent ? FLAG0_SEARCH_ECONTENT : 0)
		| (settings->warnNoBase ? FLAG0_WARN_NO_BASE : 0)
		| (((u8) settings->defaultSortMethod) << SORTMETHOD_SHIFT)
		| (((u8) settings->defaultSortDirection) << SORTDIRECTION_SHIFT)
		| (settings->allowLEDChange ? FLAG0_ALLOW_LED : 0);
	g_nsettings.lang = settings->language;
	g_nsettings.max_elogs = settings->maxExtraLogs;
	g_nsettings.wallpaper_dim = 132;
	g_nsettings.theme_path = settings->isLightMode ? SPECIAL_LIGHT : SPECIAL_DARK;
	g_nsettings.background_path.clear();

	proxy::legacy::Params p;
	proxy::legacy::parse(p);
	if(p.port != 0)
	{
		g_nsettings.proxy_username = p.username;
		g_nsettings.proxy_password = p.password;
		g_nsettings.proxy_host = p.host;
		g_nsettings.proxy_port = p.port;
	} else g_nsettings.proxy_port = 0;
	proxy::legacy::del();

	write_settings();
}

void reset_settings(bool set_default_lang)
{
	if(set_default_lang)
	{
		cfguInit(); /* ensure cfg is loaded */
		g_nsettings.lang = i18n::default_lang();
		cfguExit(); /* remove cfg reference */
	}
	else g_nsettings.lang = lang::english;

	g_nsettings.flags0 = FLAG0_RESUME_DOWNLOADS           |  FLAG0_LOAD_FREE_SPACE
	                   | FLAG0_SHOW_BATTERY               |  FLAG0_SHOW_NET
	                   | ((u64) LumaLocaleMode::automatic << LUMALOCALE_SHIFT)
	                   | ((u64) SortDirection::ascending  << SORTDIRECTION_SHIFT)
	                   | ((u64) SortMethod::none         << SORTMETHOD_SHIFT)
	                   | FLAG0_SEARCH_ECONTENT            | FLAG0_WARN_NO_BASE
	                   | FLAG0_ALLOW_LED;

	g_nsettings.max_elogs = 0; /* memory log by default */
	g_nsettings.wallpaper_dim = 132;
	g_nsettings.theme_path = SPECIAL_DARK;
	g_nsettings.background_path.clear();
	g_nsettings.proxy_port = 0; /* disable proxy by default */
	g_nsettings.migration = LATEST_MIGRATION;

	write_settings();
	g_loaded = true;
}

static void apply_migrations()
{
	if(g_nsettings.migration == LATEST_MIGRATION)
		return; /* nothing to do; this is an optimization as it's likely to already be at the latest */
	if(g_nsettings.migration < migration_default_reinstall)
		g_nsettings.flags0 |= FLAG0_DEFAULT_REINSTALL;
	if(g_nsettings.migration < migration_latency_graph)
		{ } /* this one is just here as a courtesy */
	if(g_nsettings.migration < migration_goto_region)
		g_nsettings.flags0 |= FLAG0_GOTO_REGION;
	if(g_nsettings.migration < migration_rev_reinstall)
		g_nsettings.flags0 &= ~FLAG0_DEFAULT_REINSTALL;
	if(g_nsettings.migration < migration_enable_mem_log)
		g_nsettings.max_elogs = 0;
	/* in the future more migrations may be written here */
	g_nsettings.migration = LATEST_MIGRATION;
	write_settings();
}

/* returns false on failure */
static bool parse_string(std::string& res, u8 *buf, size_t& offset, size_t len)
{
	if(offset + sizeof(u16) > len) return false;
	u16 slen = * (u16 *) &buf[offset];
	offset += sizeof(u16);
	if(offset + slen > len) return false;
	res = std::string((char *) &buf[offset], slen);
	offset += slen;
	return true;
}

/*
everything LE

lumalocale_e : u8 {
	disabled
	automatic
	manual
}

enum sortmethod_e : u8 {
	// ...
}

enum sortdirection_e : u8 {
	asc
	desc
}

enum lang_e : u8 {
	// ...
}

bitfield flags0_b : u64 {
	FLAG0_RESUME_DOWNLOADS
	FLAG0_LOAD_FREE_SPACE
	FLAG0_SHOW_BATTERY
	FLAG0_SHOW_NET
	FLAG0_BAD_TIME_FORMAT // 12-hour instead of 24-hour
	FLAG0_PROGBAR_TOP // progress bar on top screen instead of bottom
	FLAG0_LUMALOCALE0 // pair with FLAG0_LUMALOCALE1, see lumalocale_e
	FLAG0_LUMALOCALE1 // pair with FLAG0_LUMALOCALE0, see lumalocale_e
	FLAG0_SORTDIRECTION0 // enum sortdirection_e
	FLAG0_SORTMETHOD0
	FLAG0_SORTMETHOD1
	FLAG0_SORTMETHOD2
	FLAG0_SORTMETHOD3
	FLAG0_ECONTENT
	FLAG0_BASE_WARN
	FLAG0_LED
	FLAG0_DEFAULT_REINSTALL
	FLAG0_ALT_NAME
}

struct dynstr {
	u16 len
	char data[len]
}

struct ths_settings {
	char[4] magic // "4HSS"
	flags0_b flags0
	lang_e lang
	u8 elogs
	u8 migration_num
	u8[3] reserved2
	u32[3] reserved1
	dynstr theme_path
	u16 proxy_port
	dynstr proxy_host     // if proxy_port != 0
	dynstr proxy_username // if proxy_port != 0
	dynstr proxy_password // if proxy_port != 0
}
*/

bool ensure_settings()
{
	if(g_loaded) return false; /* finished */
	g_loaded = true; /* we'll always write _something_ to settings after this point */

	bool ret = false;

	FILE *settings_f = fopen(SETTINGS_LOCATION, "r");
	if(!settings_f) { reset_settings(true); return true; }
	fseek(settings_f, 0, SEEK_END);
	size_t size = ftell(settings_f);
	fseek(settings_f, 0, SEEK_SET);
	u8 *buf = NULL;
	size_t offset;
	/* invalid file */
	if(size < 34) goto default_settings;
	buf = (u8 *) malloc(size);
	panic_assert(buf, "failed to allocate buffer for settings");
	panic_assert(fread(buf, size, 1, settings_f) == 1, "failed to read settings");
	if(memcmp(buf, "3HSS", 4) == 0)
	{
		/* legacy format; migrate to new format */
		migrate_settings(buf);
		goto out;
	}
	if(memcmp(buf, "4HSS", 4) != 0)
	{
		/* invalid magic; re-write file */
		goto default_settings;
	}
	g_nsettings.flags0 = * (u64 *) &buf[0x04];
	g_nsettings.lang = buf[0x0C];
	/* TODO: Check validity of lang */
	g_nsettings.max_elogs = buf[0x0D];
	g_nsettings.migration = buf[0x0E];
	g_nsettings.wallpaper_dim = buf[0x0F];
	/* Reserved bytes were zero in settings written before this option. */
	if(g_nsettings.wallpaper_dim < 64)
		g_nsettings.wallpaper_dim = 132;
	/* start parsing strings */
	offset = 0x1E;
	if(!parse_string(g_nsettings.theme_path, buf, offset, size)) goto default_settings;
	if(offset + sizeof(u16) > size) goto default_settings;
	g_nsettings.proxy_port = * (u16 *) &buf[offset];
	offset += sizeof(u16);
	if(ISET_PROXY_ENABLED)
	{
		if (!g_nsettings.proxy_port) goto default_settings;
		if(!parse_string(g_nsettings.proxy_host, buf, offset, size)) goto default_settings;
		if(!parse_string(g_nsettings.proxy_username, buf, offset, size)) goto default_settings;
		if(!parse_string(g_nsettings.proxy_password, buf, offset, size)) goto default_settings;
	}
	/* Added by Nocturne. Old settings files end before this optional field. */
	if(offset < size)
	{
		if(!parse_string(g_nsettings.background_path, buf, offset, size))
			goto default_settings;
	}
	else g_nsettings.background_path.clear();

	apply_migrations();

	goto out;
default_settings:
	reset_settings(true);
	ret = true;
out:
	fclose(settings_f);
	free((void *) buf);
	return ret;
}

bool settings_are_ready()
{ return g_loaded; }

bool should_show_background_tip()
{ return !ISET_BACKGROUND_TIP_SEEN; }

void mark_background_tip_seen()
{
	g_nsettings.flags0 |= FLAG0_BACKGROUND_TIP_SEEN;
	write_settings();
}

void cleanup_themes()
{
	for(ui::Theme& theme : g_avail_themes)
		theme.cleanup();
}

std::vector<ui::Theme>& themes()
{ return g_avail_themes; }


typedef struct SettingInfo
{
	str::type name;
	str::type desc;
	SettingsId ID;
	bool as_text;
} SettingInfo;

static const char *localemode2str(LumaLocaleMode mode)
{
	switch(mode)
	{
	case LumaLocaleMode::manual: return STRING(manual);
	case LumaLocaleMode::automatic: return STRING(automatic);
	case LumaLocaleMode::disabled: return STRING(disabled);
	}
	return STRING(unknown);
}

static const char *localemode2str_en(LumaLocaleMode mode)
{
	switch(mode)
	{
	case LumaLocaleMode::manual: return "manual";
	case LumaLocaleMode::automatic: return "automatic";
	case LumaLocaleMode::disabled: return "disabled";
	}
	return "unknown";
}

static const char *direction2str(SortDirection dir)
{
	switch(dir)
	{
	case SortDirection::ascending: return STRING(ascending);
	case SortDirection::descending: return STRING(descending);
	}
	return STRING(unknown);
}

static const char *direction2str_en(SortDirection dir)
{
	switch(dir)
	{
	case SortDirection::ascending: return "ascending";
	case SortDirection::descending: return "descending";
	}
	return "unknown";
}

static const char *method2str(SortMethod dir)
{
	switch(dir)
	{
	case SortMethod::alpha: return STRING(alphabetical);
	case SortMethod::tid: return STRING(tid);
	case SortMethod::size: return STRING(size);
	case SortMethod::downloads: return STRING(downloads);
	case SortMethod::id: return STRING(landing_id);
	case SortMethod::none: return STRING(none);
	}
	return "unknown";
}

static const char *method2str_en(SortMethod dir)
{
	switch(dir)
	{
	case SortMethod::alpha: return "alphabetical";
	case SortMethod::tid: return "title ID";
	case SortMethod::size: return "size";
	case SortMethod::downloads: return "downloads";
	case SortMethod::id: return "hShop ID";
	case SortMethod::none: return "(none)";
	}
	return "unknown";
}

static bool serialize_id_bool(SettingsId ID)
{
	switch(ID)
	{
	case ID_Resumable:
		return ISET_RESUME_DOWNLOADS;
	case ID_FreeSpace:
		return ISET_LOAD_FREE_SPACE;
	case ID_Battery:
		return ISET_SHOW_BATTERY;
	case ID_Extra:
		return ISET_SEARCH_ECONTENT;
	case ID_WarnNoBase:
		return ISET_WARN_NO_BASE;
	case ID_AllowLED:
		return ISET_ALLOW_LED;
	case ID_Reinstall:
		return ISET_DEFAULT_REINSTALL;
	case ID_ShowAlt:
		return ISET_SHOW_ALT;
	case ID_DisGraph:
		return ISET_DISABLE_GRAPH;
	case ID_GotoRegion:
		return ISET_GOTO_REGION;
	case ID_TimeFmt:
	case ID_ProgLoc:
	case ID_Language:
	case ID_Localemode:
	case ID_Proxy:
	case ID_MaxELogs:
	case ID_Method:
	case ID_Direction:
	case ID_ProxyEnabled:
	case ID_Background:
	case ID_WallpaperDim:
	case ID_Performance:
		panic("impossible bool setting switch case reached");
	}

	return false;
}

static std::string serialize_id_text(SettingsId ID)
{
	switch(ID)
	{
	case ID_Resumable:
	case ID_FreeSpace:
	case ID_Battery:
	case ID_Extra:
	case ID_WarnNoBase:
	case ID_AllowLED:
	case ID_Reinstall:
	case ID_ShowAlt:
	case ID_DisGraph:
	case ID_GotoRegion:
	case ID_ProxyEnabled:
		panic("impossible text setting switch case reached");
	case ID_TimeFmt:
		return ISET_BAD_TIME_FORMAT ? STRING(fmt_12h) : STRING(fmt_24h);
	case ID_ProgLoc:
		return ISET_PROGBAR_TOP ? STRING(top) : STRING(bottom);
	case ID_Language:
		return i18n::langname(g_nsettings.lang);
	case ID_Localemode:
		return localemode2str(SETTING_LUMALOCALE);
	case ID_Proxy:
		return ISET_PROXY_ENABLED ? STRING(enabled) : STRING(disabled);
	case ID_MaxELogs:
		return std::to_string(g_nsettings.max_elogs);
	case ID_Direction:
		return direction2str(SETTING_DEFAULT_SORTDIRECTION);
	case ID_Method:
		return method2str(SETTING_DEFAULT_SORTMETHOD);
	case ID_Background:
		if(g_nsettings.background_path.empty())
			return STRING(none);
		{
			size_t slash = g_nsettings.background_path.find_last_of("/\\");
			return slash == std::string::npos
				? g_nsettings.background_path
				: g_nsettings.background_path.substr(slash + 1);
		}
	case ID_WallpaperDim:
		return std::to_string((g_nsettings.wallpaper_dim * 100 + 127) / 255) + "%";
	case ID_Performance:
		return "804 MHz + L2 + Core 2";
	}

	return STRING(unknown);
}

template <typename TEnum>
static void read_set_enum(const std::vector<std::string>& keys,
	const std::vector<TEnum>& values, TEnum& val)
{
	ui::I18NEnabledRenderQueue queue;

	ui::Selector<TEnum> *sel;
	ui::builder<ui::Selector<TEnum>>(ui::Screen::bottom, keys, values, &val)
		.add_to(&sel, queue);
	sel->search_set_idx(val);

	queue.render_finite();
}

static void clean_proxy_content(std::string& s)
{
	if(s.find("https://") == 0)
		s.replace(0, 8, "");
	if(s.find("http://") == 0)
		s.replace(0, 7, "");
	std::string::size_type pos;
	while((pos = s.find(":")) != std::string::npos)
		s.replace(pos, 1, "");
	while((pos = s.find("\n")) != std::string::npos)
		s.replace(pos, 1, "");
}

static void show_update_proxy()
{
	ui::I18NEnabledRenderQueue queue;

	ui::Button *password;
	ui::Button *username;
	ui::Button *host;
	ui::Button *port;

	constexpr float w = ui::screen_width(ui::Screen::bottom) - 20.0f;
	constexpr float h = 20;

#define UPDATE_LABELS() do { \
		port->set_label(g_nsettings.proxy_port == 0 ? "" : std::to_string(g_nsettings.proxy_port)); \
		password->set_label(g_nsettings.proxy_password); \
		username->set_label(g_nsettings.proxy_username); \
		host->set_label(g_nsettings.proxy_host); \
	} while(0)

#define BASIC_CALLBACK(name,initial_value) \
		[&name](ui::Button *) -> bool { \
			ui::RenderQueue::global()->render_and_then([&name]() -> void { \
				SwkbdButton btn; \
				std::string val = ui::keyboard([](ui::AppletSwkbd *swkbd) -> void { \
					swkbd->hint(STRING(name)); \
					swkbd->init_text(initial_value); \
				}, &btn); \
			\
				if(btn == SWKBD_BUTTON_CONFIRM) \
				{ \
					clean_proxy_content(val); \
					g_nsettings.proxy_##name = val; \
					name->set_label(val); \
				} \
			}); \
			return true; \
		}
		
	ui::Text *abled_text = nullptr;
	
	ui::builder<ui::Toggle>(ui::Screen::bottom, ISET_PROXY_ENABLED,
		[&abled_text]() -> void {
			update_settings_ID(ID_ProxyEnabled);
			abled_text->set_text(ISET_PROXY_ENABLED ? str::enabled : str::disabled);
		})
		.x(10.0f)
		.y(ui::layout::bottom)
		.add_to(queue);
	
	ui::builder<ui::Text>(ui::Screen::bottom, ISET_PROXY_ENABLED ? str::enabled : str::disabled)
		.right(queue.back(), 10.0f)
		.y(ui::layout::bottom)
		.add_to(&abled_text, queue);
	
	ui::builder<ui::Text>(ui::Screen::top, str::proxy_toggle_hint)
		.wrap()
		.x(ui::layout::center_x)
		.y(ui::layout::center_y)
		.add_to(queue);

	/* host */
	ui::builder<ui::Text>(ui::Screen::bottom, str::host)
		.x(10.0f)
		.y(10.0f)
		.add_to(queue);
	ui::builder<ui::Button>(ui::Screen::bottom)
		.x(10.0f)
		.when_clicked(BASIC_CALLBACK(host, g_nsettings.proxy_host))
		.size(w, h)
		.under(queue.back())
		.add_to(&host, queue);
	/* port */
	ui::builder<ui::Text>(ui::Screen::bottom, str::port)
		.x(10.0f)
		.under(queue.back())
		.add_to(queue);
	ui::builder<ui::Button>(ui::Screen::bottom)
		.x(10.0f)
		.when_clicked([&port](ui::Button *) -> bool {
			ui::RenderQueue::global()->render_and_then([&port]() -> void {
				SwkbdButton btn;
				uint64_t val = ui::numpad([](ui::AppletSwkbd *swkbd) -> void {
					swkbd->hint(STRING(port));
				}, &btn, nullptr, 5 /* max is 65535: 5 digits */);

				if(btn == SWKBD_BUTTON_CONFIRM && val != 0)
				{
					val &= 0xFFFF; /* limit to a max of 0xFFFF */
					g_nsettings.proxy_port = val;
					port->set_label(std::to_string(val));
				}
			});
			return true;
		})
		.size(w, h)
		.under(queue.back())
		.add_to(&port, queue);
	/* username */
	ui::builder<ui::Text>(ui::Screen::bottom, str::username)
		.x(10.0f)
		.under(queue.back())
		.add_to(queue);
	ui::builder<ui::Button>(ui::Screen::bottom)
		.x(10.0f)
		.when_clicked(BASIC_CALLBACK(username, g_nsettings.proxy_username))
		.size(w, h)
		.under(queue.back())
		.add_to(&username, queue);
	/* password */
	ui::builder<ui::Text>(ui::Screen::bottom, str::password)
		.x(10.0f)
		.under(queue.back())
		.add_to(queue);
	ui::builder<ui::Button>(ui::Screen::bottom)
		.x(10.0f)
		.when_clicked(BASIC_CALLBACK(password, g_nsettings.proxy_password))
		.size(w, h)
		.under(queue.back())
		.add_to(&password, queue);

	ui::Button *clear_btn = nullptr;
	ui::builder<ui::Button>(ui::Screen::bottom, str::clear)
		.when_clicked([host, port, username, password](ui::Button *) -> bool {
			g_nsettings.proxy_username = "";
			g_nsettings.proxy_password = "";
			g_nsettings.proxy_host = "";
			g_nsettings.proxy_port = 0;
			UPDATE_LABELS();
			return true;
		})
		.x(ui::layout::right)
		.y(ui::layout::bottom)
		.wrap()
		.add_to(&clear_btn, queue);
	
	clear_btn->set_x(clear_btn->get_x() - clear_btn->width());
	clear_btn->set_y(clear_btn->get_y() - clear_btn->height());

	UPDATE_LABELS();
	queue.render_finite_button(KEY_B | KEY_A);
#undef BASIC_CALLBACK
#undef UPDATE_LABELS
}

static void show_elogs()
{
	uint64_t val;
	do {
		SwkbdButton btn;
		val = ui::numpad([](ui::AppletSwkbd *swkbd) -> void {
			swkbd->hint(STRING(elogs_hint));
		}, &btn, nullptr, 3 /* max is 255: 3 digits */);

		if(btn != SWKBD_BUTTON_CONFIRM)
			return;
		if(val <= 0xFF)
			break;
	} while(true);
	g_nsettings.max_elogs = val & 0xFF;
}

static bool supported_background_name(const std::string& name)
{
	size_t dot = name.find_last_of('.');
	if(dot == std::string::npos) return false;
	std::string ext = name.substr(dot);
	std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
		return (char) tolower(c);
	});
	return ext == ".png" || ext == ".jpg" || ext == ".jpeg";
}

static void show_background_menu()
{
	mkdir("/3ds", 0777);
	mkdir("/3ds/3hs", 0777);
	mkdir(BACKGROUNDS_DIR, 0777);

	struct BackgroundEntry {
		std::string label;
		std::string path;
	};
	std::vector<BackgroundEntry> entries = {
		{ STRING(none), "" }
	};

	DIR *dir = opendir(BACKGROUNDS_DIR);
	if(dir)
	{
		struct dirent *ent;
		while((ent = readdir(dir)))
		{
			std::string name = ent->d_name;
			if(name == "." || name == ".." || !supported_background_name(name))
				continue;
			entries.push_back({ name, std::string(BACKGROUNDS_DIR) + name });
		}
		closedir(dir);
	}
	std::sort(entries.begin() + 1, entries.end(),
		[](const BackgroundEntry& a, const BackgroundEntry& b) {
			return a.label < b.label;
		});

	ui::I18NEnabledRenderQueue queue;
	ui::MenuSelect *menu;
	ui::builder<ui::Text>(ui::Screen::top,
			"SD / 3ds / 3hs / backgrounds")
		.size(0.45f)
		.x(ui::layout::center_x)
		.y(36.0f)
		.wrap()
		.add_to(queue);
	ui::builder<ui::Text>(ui::Screen::top,
			"PNG + JPEG  |  cover crop  |  400x240 / 320x240")
		.size(0.40f)
		.x(ui::layout::center_x)
		.under(queue.back(), 5.0f)
		.wrap()
		.add_to(queue);

	ui::builder<ui::MenuSelect>(ui::Screen::bottom)
		.add_to(&menu, queue);

	for(size_t i = 0; i < entries.size(); ++i)
	{
		menu->add_row(entries[i].label, [i, &entries]() -> bool {
			const std::string& path = entries[i].path;
			if(ui::set_user_background(path))
			{
				g_nsettings.background_path = path;
				write_settings();
				return false;
			}
			ui::notice("Could not load that background image.");
			return true;
		});
	}

	size_t selected = 0;
	for(size_t i = 1; i < entries.size(); ++i)
		if(entries[i].path == g_nsettings.background_path)
			selected = i;
	menu->set_pos(selected);
	queue.render_finite_button(KEY_B);
}


SortMethod settings_sort_switch()
{
	SortMethod ret = SETTING_DEFAULT_SORTMETHOD;
	read_set_enum<SortMethod>(
		{ STRING(alphabetical), STRING(tid), STRING(size), STRING(downloads), STRING(landing_id), STRING(none) },
		{ SortMethod::alpha, SortMethod::tid, SortMethod::size, SortMethod::downloads, SortMethod::id, SortMethod::none},
		ret
	);
	return ret;
}

void show_set_language_core()
{
	ui::I18NEnabledRenderQueue queue;
	ui::MenuSelect *ms;
	lang::type selected_id = lang::_i_max;

	ui::builder<ui::MenuSelect>(ui::Screen::bottom)
		.add_to(&ms, queue);

#define ITER(name, id) \
	ms->add_row(name, [&selected_id]() -> bool { g_nsettings.lang = id; selected_id = id; return false; });
	I18N_ALL_LANG_ITER(ITER)
#undef ITER

	queue.render_finite_button(KEY_B);

	bool selected = selected_id != lang::_i_max;

	if (selected_id != lang::_i_max) {
		ui::RenderQueue::global()->before_render([selected_id]() -> void {
			ui::trigger_i18n_update(selected_id);
		});
	}
}

void show_set_language()
{
	bool focus = ui::set_focus(true);
	show_set_language_core();
	ui::set_focus(focus);
}

static void update_settings_ID(SettingsId ID)
{
	switch(ID)
	{
	// Boolean
	case ID_Resumable:
		g_nsettings.flags0 ^= FLAG0_RESUME_DOWNLOADS;
		break;
	case ID_WarnNoBase:
		g_nsettings.flags0 ^= FLAG0_WARN_NO_BASE;
		break;
	case ID_FreeSpace:
		g_nsettings.flags0 ^= FLAG0_LOAD_FREE_SPACE;
		// If we switched it from off to on and we've never drawed the widget before
		// It wouldn't draw the widget until another update
		ui::RenderQueue::global()->find_tag<ui::FreeSpaceIndicator>(ui::tag::free_indicator)->update();
		break;
	case ID_Battery:
		g_nsettings.flags0 ^= FLAG0_SHOW_BATTERY;
		break;
	case ID_Extra:
		g_nsettings.flags0 ^= FLAG0_SEARCH_ECONTENT;
		break;
	case ID_AllowLED:
		g_nsettings.flags0 ^= FLAG0_ALLOW_LED;
		break;
	case ID_Reinstall:
		g_nsettings.flags0 ^= FLAG0_DEFAULT_REINSTALL;
		break;
	case ID_ShowAlt:
		g_nsettings.flags0 ^= FLAG0_SHOW_ALT;
		break;
	case ID_DisGraph:
		g_nsettings.flags0 ^= FLAG0_DISABLE_GRAPH;
		break;
	case ID_GotoRegion:
		g_nsettings.flags0 ^= FLAG0_GOTO_REGION;
		break;
	case ID_ProxyEnabled:
		g_nsettings.flags0 ^= FLAG0_PROXY_ENABLED;
		break;
	// Enums
	case ID_TimeFmt:
	{
		enum class Timefmt { good, bad }
			val = ISET_BAD_TIME_FORMAT ? Timefmt::bad : Timefmt::good;
		read_set_enum<Timefmt>(
			{ STRING(fmt_24h), STRING(fmt_12h) },
			{ Timefmt::good, Timefmt::bad },
			val
		);
		if(val == Timefmt::good)
			g_nsettings.flags0 &= ~FLAG0_BAD_TIME_FORMAT;
		else if(val == Timefmt::bad)
			g_nsettings.flags0 |= FLAG0_BAD_TIME_FORMAT;
		break;
	}
	case ID_ProgLoc:
	{
		enum class ProgressBarLocation { top, bottom }
			val = ISET_PROGBAR_TOP ? ProgressBarLocation::top : ProgressBarLocation::bottom;
		read_set_enum<ProgressBarLocation>(
			{ STRING(top), STRING(bottom) },
			{ ProgressBarLocation::top, ProgressBarLocation::bottom },
			val
		);
		if(val == ProgressBarLocation::top)
			g_nsettings.flags0 |= FLAG0_PROGBAR_TOP;
		else if(val == ProgressBarLocation::bottom)
			g_nsettings.flags0 &= ~FLAG0_PROGBAR_TOP;
		break;
	}
	case ID_Language:
		show_set_language_core();
		break;
	case ID_Localemode:
	{
		LumaLocaleMode mode = SETTING_LUMALOCALE;
		read_set_enum<LumaLocaleMode>(
			{ STRING(automatic), STRING(manual), STRING(disabled) },
			{ LumaLocaleMode::automatic, LumaLocaleMode::manual, LumaLocaleMode::disabled },
			mode
		);
		g_nsettings.flags0 = (g_nsettings.flags0 & ~(FLAG0_LUMALOCALE0 | FLAG0_LUMALOCALE1))
		                   | ((u64) mode << LUMALOCALE_SHIFT);
		break;
	}
	case ID_Direction:
	{
		SortDirection mode = SETTING_DEFAULT_SORTDIRECTION;
		read_set_enum<SortDirection>(
			{ STRING(ascending), STRING(descending) },
			{ SortDirection::ascending, SortDirection::descending },
			mode
		);
		g_nsettings.flags0 = (g_nsettings.flags0 & ~(FLAG0_SORTDIRECTION0))
		                   | ((u64) mode << SORTDIRECTION_SHIFT);
		break;
	}
	case ID_Method:
	{
		SortMethod mode = settings_sort_switch();
		g_nsettings.flags0 = (g_nsettings.flags0 & ~(FLAG0_SORTMETHOD0 | FLAG0_SORTMETHOD1 | FLAG0_SORTMETHOD2 | FLAG0_SORTMETHOD3))
		                   | ((u64) mode << SORTMETHOD_SHIFT);
		break;
	}
	// Other
	case ID_Proxy:
		show_update_proxy();
		break;
	case ID_MaxELogs:
		show_elogs();
		break;
	case ID_Background:
		show_background_menu();
		break;
	case ID_WallpaperDim:
	{
		u8 dim = g_nsettings.wallpaper_dim;
		read_set_enum<u8>(
			{ "Bright · 25%", "Balanced · 52%", "Dim · 66%", "Dark · 80%" },
			{ 64, 132, 168, 204 },
			dim
		);
		g_nsettings.wallpaper_dim = dim;
		break;
	}
	case ID_Performance:
		ui::notice("New 3DS performance mode is active.\n\n804 MHz CPU  ·  L2 cache\nCore 2 CIA writer  ·  1 MiB pipeline\nDirect CDN connection");
		break;
	}
}

void fix_sort_settings()
{
	/* basically just set sort settings to default, why is this necessairy sometimes? */
	g_nsettings.flags0 = (g_nsettings.flags0 & ~(FLAG0_SORTDIRECTION0 | FLAG0_SORTMETHOD0 | FLAG0_SORTMETHOD1 | FLAG0_SORTMETHOD2 | FLAG0_SORTMETHOD3))
	                   | ((u64) SortDirection::ascending << SORTDIRECTION_SHIFT)
	                   | ((u64) SortMethod::alpha << SORTMETHOD_SHIFT);
	write_settings();
}

void log_settings()
{
#define BOOL(n) n ? "true" : "false"
	ilog("settings dump: "
		"resumeDownloads: %s, "
		"loadFreeSpace: %s, "
		"showBattery: %s, "
		"showNet: %s, "
		"badTimeFormat: %s, "
		"progloc: %s, "
		"language: %s, "
		"lumalocalemode: %s, "
		"checkForExtraContent: %s, "
		"warnNoBase: %s, "
		"maxExtraLogs: %u, "
		"defaultSortMethod: %s, "
		"defaultSortDirection: %s, "
		"proxyEnabled: %s, "
		"themePath: %s, "
		"disableGraph: %s, "
		"backgroundPath: %s, "
		"wallpaperDim: %u, "
		"performanceMode: new3ds",
			BOOL(ISET_RESUME_DOWNLOADS), BOOL(ISET_LOAD_FREE_SPACE),
			BOOL(ISET_SHOW_BATTERY), BOOL(ISET_SHOW_NET), BOOL(ISET_BAD_TIME_FORMAT),
			ISET_PROGBAR_TOP ? "top" : "bottom",
			i18n::langname(g_nsettings.lang), localemode2str_en(SETTING_LUMALOCALE),
			BOOL(ISET_SEARCH_ECONTENT), BOOL(ISET_WARN_NO_BASE), g_nsettings.max_elogs,
			method2str_en(SETTING_DEFAULT_SORTMETHOD), direction2str_en(SETTING_DEFAULT_SORTDIRECTION),
			BOOL(g_nsettings.proxy_port != 0), g_nsettings.theme_path.c_str(), BOOL(ISET_DISABLE_GRAPH),
			g_nsettings.background_path.empty() ? "(none)" : g_nsettings.background_path.c_str(),
			g_nsettings.wallpaper_dim);
#undef BOOL
}

static void display_setting_value(const SettingInfo& set, ui::Text *value, ui::Toggle *toggle)
{
	if(set.as_text)
	{
		value->set_text(PSTRING(value_x, serialize_id_text(set.ID)));
		value->set_hidden(false);
		toggle->set_hidden(true);
		return;
	}

	value->set_hidden(true);
	toggle->set_hidden(false);
	toggle->set_toggled(serialize_id_bool(set.ID));
}

void show_settings()
{
	std::vector<SettingInfo> settingsInfo =
	{
		{ str::resume_dl      , str::resume_dl_desc      , ID_Resumable  , false },
		{ str::load_space     , str::load_space_desc     , ID_FreeSpace  , false },
		{ str::show_battery   , str::show_battery_desc   , ID_Battery    , false },
		{ str::check_extra    , str::check_extra_desc    , ID_Extra      , false },
		{ str::warn_no_base   , str::warn_no_base_desc   , ID_WarnNoBase , false },
		{ str::allow_led      , str::allow_led_desc      , ID_AllowLED   , false },
		{ str::default_reinst , str::default_reinst_desc , ID_Reinstall  , false },
		{ str::show_alt       , str::show_alt_desc       , ID_ShowAlt    , false },
		{ str::disable_graph  , str::disable_graph_desc  , ID_DisGraph   , false },
		{ str::goto_region    , str::goto_region_desc    , ID_GotoRegion , false },
		{ str::time_format    , str::time_format_desc    , ID_TimeFmt    , true  },
		{ str::progbar_screen , str::progbar_screen_desc , ID_ProgLoc    , true  },
		{ str::language       , str::language_desc       , ID_Language   , true  },
		{ str::lumalocalemode , str::lumalocalemode_desc , ID_Localemode , true  },
		{ str::proxy          , str::proxy_desc          , ID_Proxy      , true  },
		{ str::max_elogs      , str::max_elogs_desc      , ID_MaxELogs   , true  },
		{ str::def_sort_meth  , str::def_sort_meth_desc  , ID_Method     , true  },
		{ str::def_sort_dir   , str::def_sort_dir_desc   , ID_Direction  , true  },
		{ str::background_image, str::background_image_desc, ID_Background, true },
		{ str::wallpaper_dimming, str::wallpaper_dimming_desc, ID_WallpaperDim, true },
		{ str::performance_mode, str::performance_mode_desc, ID_Performance, true },
	};

	panic_assert(settingsInfo.size() > 0, "empty settings meta table");

	using list_t = ui::List<SettingInfo>;

	bool focus = ui::set_focus(true);
	bool dirty = false;

	SettingsId current_setting = settingsInfo[0].ID;
	ui::I18NEnabledRenderQueue queue;
	ui::Toggle *toggle;
	ui::Text *value;
	ui::Text *desc;
	list_t *list;

	std::function<void(list_t *self, size_t i)> cb = [&value, &toggle, &desc, &current_setting](list_t *self, size_t i) -> void {
		const SettingInfo& set = self->at(i);
		current_setting = set.ID;
		display_setting_value(set, value, toggle);
		desc->set_text(set.desc);
		ui::BaseWidget *display = set.as_text
			? (ui::BaseWidget *) value : (ui::BaseWidget *) toggle;
		display->set_y(ui::under(desc, display, 5.0f));
	};

	ui::builder<ui::Text>(ui::Screen::bottom, settingsInfo[0].desc)
		.x(10.0f)
		.y(20.0f)
		.wrap()
		.add_to(&desc, queue);
	ui::builder<ui::Text>(ui::Screen::bottom)
		.x(10.0f)
		.under(desc, 5.0f)
		.wrap()
		.hide()
		.add_to(&value, queue);
	ui::builder<ui::Toggle>(ui::Screen::bottom, serialize_id_bool(settingsInfo[0].ID),
			[&current_setting, &dirty]() -> void { update_settings_ID(current_setting); dirty = true; })
		.x(10.0f)
		.under(desc, 5.0f)
		.add_to(&toggle, queue);

	ui::builder<list_t>(ui::Screen::top, &settingsInfo)
		.manual_i18n_update([&cb](list_t *l, lang::type) -> void {
			l->update();
			cb(l, l->get_pos());
		})
		.to_string([](const SettingInfo& entry) -> std::string { return i18n::getstr(entry.name); })
		.when_select([value, toggle, &dirty](list_t *self, size_t i, u32 kDown) -> bool {
			(void) kDown;
			dirty = true;
			ui::RenderQueue::global()->render_and_then([self, i, value, toggle]() -> void {
				const SettingInfo& set = self->at(i);
				update_settings_ID(set.ID);
				display_setting_value(set, value, toggle);
			});
			return true;
		})
		.when_change(cb)
		.x(5.0f).y(25.0f)
		.add_to(&list, queue);

	auto reset_settings_local = [](u32) -> bool {
		ui::RenderQueue::global()->render_and_then([]() -> void {
			if(ui::Confirm::exec(str::sure_reset))
			{
				reset_settings();
				ui::set_user_background("");
				/* Perhaps this should be intergrated into reset_settings()?
				 * only other place it's used is main() where nothing is loaded yet anyway */
				ui::ThemeManager::global()->reget();
				ui::RenderQueue::global()->find_tag<ui::FreeSpaceIndicator>(ui::tag::free_indicator)->update();
			}
		});
		return true;
	};

	ui::builder<ui::ButtonCallback>(ui::Screen::top, KEY_R)
		.when_kdown(reset_settings_local)
		.add_to(queue);

	queue.render_finite_button(KEY_B);
	ui::set_focus(focus);

	if(dirty)
	{
		log_on_settings_changed();
		write_settings();
	}
}

#include "build/light_hstx.h"
#include "build/dark_hstx.h"

/* due to pushing changing the pointers of the vector ... */
static ui::Theme *get_light_theme()
{
	return &g_avail_themes[light_theme_index];
}

void load_current_theme()
{
	ui::Theme cthem;

	ui::Theme *light_theme;

	/* Nocturne owns its visual system. Legacy/custom 3hs themes can otherwise
	 * leak unrelated colours into only one screen while our custom surfaces
	 * stay pink. Wallpapers remain independently configurable. */
	g_nsettings.theme_path = SPECIAL_DARK;

	/* we must first load the theme we **don't** want to select */
	int firstIndex = g_nsettings.theme_path == SPECIAL_LIGHT ? 1 : 0;
	light_theme_index = firstIndex;
	int secondIndex = 1 - firstIndex;
	static const struct {
		const char *name;
		const u8 *data;
		u32 size;
	} sets[] = {
		/* keep in this order */
		{ .name = SPECIAL_LIGHT, .data = light_hstx_bin, .size = light_hstx_bin_size },
		{ .name = SPECIAL_DARK, .data = dark_hstx_bin, .size = dark_hstx_bin_size },
	};

	panic_assert(cthem.open(sets[firstIndex].data, sets[firstIndex].size, sets[firstIndex].name, nullptr), "failed to parse built-in theme");
	g_avail_themes.push_back(cthem);
	cthem.clear();

	panic_assert(cthem.open(sets[secondIndex].data, sets[secondIndex].size, sets[secondIndex].name, nullptr), "failed to parse built-in theme");
	g_avail_themes.push_back(cthem);
	cthem.clear();

	if(strncmp(g_nsettings.theme_path.c_str(), SPECIAL_PREFIX, sizeof(SPECIAL_PREFIX) - 1) != 0)
	{
		/* it's fine if this fails, we'll just take special:light in that case */
		cthem.open(g_nsettings.theme_path.c_str(), get_light_theme());
		g_avail_themes.push_back(cthem);
	}
}

static void load_themes()
{
	ui::Theme cthem;

	DIR *d = opendir(THEMES_DIR);
	/* not having a themes directory is fine as well */
	if(d)
	{
		struct dirent *ent;
		constexpr size_t dirname_len = sizeof(THEMES_DIR) - 1;
		char fname[dirname_len + sizeof(ent->d_name)] = THEMES_DIR;
		while((ent = readdir(d)))
		{
			if(ent->d_type != DT_REG) continue;
			strcpy(fname + dirname_len, ent->d_name);
			cthem.clear();
			if(ui::Theme::global()->id != fname && cthem.open(fname, get_light_theme()))
				g_avail_themes.push_back(cthem);
		}
		closedir(d);
	}
}

void show_theme_menu()
{
	static int loaded_full = false;
	if(!loaded_full)
	{
		load_themes(); /* lazy load all themes */
		loaded_full = true;
	}
	ui::I18NEnabledRenderQueue queue;
	ui::MenuSelect *ms;
	ui::Text *author, *name;

	bool focus = ui::set_focus(true);

	ui::builder<ui::Text>(ui::Screen::top, ui::Theme::global()->name)
		.x(ui::layout::center_x)
		.y(40.0f)
		.wrap()
		.add_to(&name, queue);

	ui::builder<ui::Text>(ui::Screen::top, PSTRING(made_by, ui::Theme::global()->author))
		.manual_i18n_update([](ui::Text *t, lang::type) -> void {
			t->set_text(PSTRING(made_by, ui::Theme::global()->author));
		})
		.x(ui::layout::center_x)
		.under(queue.back())
		.size(0.45f, 0.45f)
		.wrap()
		.add_to(&author, queue);

	ui::builder<ui::MenuSelect>(ui::Screen::bottom)
		.when_select([&ms]() -> bool {
			ui::Theme::global()->replace_with(g_avail_themes[ms->pos()]);
			g_nsettings.theme_path = g_avail_themes[ms->pos()].id;
			ui::ThemeManager::global()->reget();
			return true;
		})
		.when_changed([&ms, author, name]() -> bool {
			ui::Theme& theme = g_avail_themes[ms->pos()];
			author->set_text(PSTRING(made_by, theme.author));
			name->set_text(theme.name);

			author->set_y(ui::under(name, author));
			return true;
		})
		.add_to(&ms, queue);

	for(ui::Theme& theme : g_avail_themes)
		ms->add_row(theme.name);
	auto it = std::find(g_avail_themes.begin(), g_avail_themes.end(), *ui::Theme::global());
	ms->set_pos(std::distance(g_avail_themes.begin(), it));

	queue.render_finite_button(KEY_B);
	ui::set_focus(focus);
	write_settings();
}
