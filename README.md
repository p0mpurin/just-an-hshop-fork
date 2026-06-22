# Nocturne

A modern New Nintendo 3DS fork of 3hs with an OLED-focused interface, custom wallpapers, faster downloads, and quality-of-life improvements.

> Nocturne is an unofficial community fork. It is not maintained or supported by the hShop team.

## Highlights

- OLED-black and light-pink visual redesign
- Custom PNG/JPEG wallpapers from `/3ds/3hs/backgrounds/`
- Adjustable wallpaper dimming
- Redesigned title details and download interface
- Live download stage, speed, progress, and ETA
- New 3DS performance mode:
  - 804 MHz CPU and L2 cache
  - dedicated Core 2 CIA writer
  - pipelined 1 MiB network/install buffers
  - direct CDN connection
- Quiet user-initiated download cancellation
- Custom HOME Menu icon and banner
- Fork-safe tracking of official 3hs releases

## Requirements

- New Nintendo 3DS, New Nintendo 3DS XL, or New Nintendo 2DS XL
- Luma3DS custom firmware
- FBI or another CIA installer

This build deliberately targets New 3DS hardware and is not intended for Old 3DS systems.

## Installation

1. Download the latest CIA from [Releases](../../releases).
2. Install it with FBI.
3. Launch Nocturne from the HOME Menu.

You can install a newer Nocturne CIA over an existing installation. If HOME Menu artwork remains cached, reboot the console or delete and reinstall the application once.

## Wallpapers

Place `.png`, `.jpg`, or `.jpeg` files in:

```text
/3ds/3hs/backgrounds/
```

Open **Settings → Background image** to choose one, then adjust **Wallpaper dimming** for readability.

## Upstream updates

Nocturne checks the official 3hs version for compatibility. It does not install the stock 3hs CIA automatically because doing so would overwrite the fork.

When upstream publishes a new release, its source changes must be merged into Nocturne and a new Nocturne CIA must be built.

## Building

The project uses devkitARM, libctru, Citro2D/Citro3D, makerom, bannertool, Perl, and mbedTLS.

Create `source/hsapi_auth.c` from the included template using credentials you are authorized to use. Never commit API credentials.

Configure the New 3DS release target:

```sh
perl build.pl --target release --configure \
  'release,targets=cia,update_base=https://download2.erista.me/3hs,nb_base=https://hshop.erista.me/nbapi,cdn_base=http://dl.hshopusercontent.com,site_url=https://hshop.erista.me'
```

Then build:

```sh
perl build.pl --target release
```

The output is `3hs.cia`.

## Credits and license

Nocturne is maintained by **p0mpurin** and is based on 3hs by the hShop development team.

The original project credits and build documentation remain available in [README](README). This fork is distributed under the GNU General Public License v3.0; see [LICENSE](LICENSE).

