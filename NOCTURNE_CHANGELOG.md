# Rune3DS changelog

## 1.5.41

- Added experimental direct CDN socket transport for faster downloads on New 3DS
- Direct socket path now reports specific error codes per failure stage
- Polished bottom dock bar: evenly spaced buttons with clean separators
- Fixed bottom details panel overlapping with Queue button
- Rebranded built-in theme names and authors
- Improved light theme contrast and fixed invisible UI elements
- Removed stale "Direct CDN was removed" notice from settings

## 1.5.40

- Rebranded public app text, install docs, UniStore metadata, and QR assets to Rune3DS
- Moved Rune3DS app data folders to `/3ds/Rune3DS`
- Updated visible catalog labels to use generic Rune3DS wording
- Renamed the release CIA asset, FBI QR target, and UniStore download target to `Rune3DS.cia`

## 1.5.39

- Refreshed the default Rune3DS theme with a minimal black, white, and gray palette
- Redesigned shared navigation surfaces with quieter dock, list, menu, and metadata styling
- Theme, accent, background, and dimming changes now preview immediately while browsing settings

## 1.5.38

- Built release with refreshed catalog API authentication
- Removed duplicate size badges from title lists; size remains visible in title details

## 1.5.37

- Settings now organized into categories: Theming, Display, Install, System, Advanced
- Wallpaper dimming uses a slider instead of presets
- Category picker on bottom screen with descriptions on top
- Centered navigation hints across the settings screens

## 1.5.36

- Added notification LED progress feedback during downloads and installs
- LED progress moves from blue to green while installing
- Success, error, and cancel states now use distinct LED signals

## 1.5.35

- Added auto-shutdown after install option in Settings
- Console powers off automatically when install completes successfully
- Works for single installs and "Install All" from queue
- Disabled by default; enable in Settings

## 1.5.34

- Bundled 3 default wallpapers — the app looks great from first launch
- Wrap-around navigation in lists — scrolling past the end loops back to the top
- Stable release for Universal-Updater

## 1.5.33

- UniStore update test

## 1.5.32

- Centered the Universal-Updater notice and made it show only once (not every launch)
- Cleaned up old in-app self-updater code paths
- Added Universal-Updater icon to the UniStore listing

## 1.5.31

- Removed the fragile launch-time Rune3DS self-updater
- Added a startup notice directing users to update Rune3DS through Universal-Updater
- Added a custom Rune3DS UniStore that installs the latest GitHub release CIA

## 1.5.30

- Validation build for the stock-style static Rune3DS updater endpoint
- Kept `/version` and the release CIA as the in-app update transport

## 1.5.29

- Switched Rune3DS's static updater check from `/version.txt` to stock-style `/version`
- Kept the static release CIA install URL for the current updater host

## 1.5.28

- Made failed startup update checks non-intrusive so transient HTTP errors do not show a boot popup
- Kept updater failure details in logs while preserving install failure warnings

## 1.5.27

- Returned Rune3DS updates to a stock-style static HTTP endpoint
- Kept detailed updater diagnostics and completed-response timeout handling

## 1.5.26

- Read the latest Rune3DS version from the GitHub latest release CIA redirect
- Avoided the GitHub `/releases/latest` page path that fails on the 3DS HTTP service

## 1.5.25

- Used GitHub's latest-release redirect tag as the Rune3DS version source
- Removed the in-app update check's dependency on the separate `rune3ds-version` release asset

## 1.5.24

- Switched the Rune3DS updater back to GitHub's latest release assets
- Kept detailed startup updater diagnostics visible for failed update checks

## 1.5.23

- Accepted 3DS HTTP receive timeouts when progress shows the full known-length response has arrived
- Prevented successful HTTP cleanup from overwriting the last failure diagnostic

## 1.5.22

- Added detailed on-screen diagnostics for startup updater HTTP failures
- Showed the compiled Rune3DS update base and build version in update check errors

## 1.5.21

- Stopped known-length HTTP downloads as soon as the advertised response body is complete
- Fixed tiny startup version checks that could time out after receiving the whole file

## 1.5.20

- Fixed launch update checks against tiny static `version.txt` responses on the 3DS HTTP service
- Kept HTTP receive requests bounded to the remaining response size so short version files do not time out

## 1.5.19

- Moved Rune3DS's in-app updater to the Vercel static update endpoint
- Mirrored the updater CIA in the Vercel site so the 3DS no longer depends on GitHub release downloads

## 1.5.18

- Switched update metadata back to the tiny GitHub release asset now that long redirects are supported
- Avoided the GitHub Releases API response that fails on the 3DS HTTP service

## 1.5.17

- Fixed GitHub Releases API version parsing when JSON contains whitespace
- Added a visible notice if Rune3DS update checking fails

## 1.5.16

- Tiny release for testing the visible in-app updater prompt and install flow

## 1.5.15

- Switched Rune3DS update detection to the GitHub Releases API to avoid raw-file cache lag

## 1.5.14

- Fixed GitHub updater fetches by avoiding long release-asset redirects for version checks
- Increased HTTP redirect URL capacity for GitHub release asset downloads
- Added a visible update confirmation prompt when a Rune3DS update is found

## 1.5.13

- Small polish release for validating the GitHub-based in-app updater

## 1.5.12

- Added Rune3DS launch-time self-updates from GitHub Releases
- Kept compatibility update checks non-destructive so stock CIAs do not overwrite the fork
- Fixed 800px top-screen mode UI layout so widgets stay fitted to the visible 400px screen area

## 1.5.11

- Added size badges to the title browse list
- Added queue reordering with [L]/[R] buttons
- Refined dark theme palette with improved contrast and warmth
- Improved MenuSelect, List, and PopUp animations and visual polish
- Polished progress bar, toggle, checkbox, and button widgets
- Increased DirectCdnDownload recv buffer from 256 KiB to 1 MiB (4× fewer syscalls)
- Added DNS caching for the direct socket path (avoids repeated getaddrinfo calls)
- Added TCP_NODELAY and doubled SO_RCVBUF for smoother TCP throughput
- Increased CIA write pipeline from 2 to 3 slots for better write-variance tolerance
- Removed panic_assert on partial HTTPC chunks (no more random crash deaths)
- Pre-fetch CDN download URL while user browses title details (saves ~500ms on install start)
- **Made 800px top-screen mode fully functional:**
  - Wallpapers now render at 800px when wide mode is active (were stuck at 400px)
  - Theme background sprites are centered in 800px framebuffer
  - Old 3DS/2DS hardware is blocked from enabling the mode (hardware doesn't support it)
  - Failure to allocate the 800px framebuffer is now logged
  - Proper hardware capability check prevents corrupted display on unsupported devices

## 1.2.2

- Removed decorative frosted bars that could appear empty after changing screens
- Added an experimental 800px top-screen display option for real-hardware testing

## 1.2.1

- Fixed text and donation link overlap on the game installation screen

## 1.2.0

- Added eased selection animation to lists and command menus
- Added smooth popup lift/fade entrances
- Added interpolated progress-bar movement
- Replaced blurry private-use button glyphs with crisp `[A]`, `[B]`, and related badges
- Snapped text rendering to the physical pixel grid
- Increased minimum text sizes and improved status-line readability
- Fixed title-detail, metadata, action-hint, download-screen, and free-space text overlap
- Improved long-title and alternative-title wrapping
- Updated the startup notice to identify Rune3DS as an unofficial fork
- Clarified that compatibility updates do not update Rune3DS

## 1.1.0

- Added an optional New 3DS direct-socket CDN transport
- Kept NBAPI authentication on Nintendo's stable HTTP service
- Added automatic fallback when the direct CDN connection cannot start
- Added bounded connection timeouts and responsive download cancellation
- Added an authenticated, network-only catalog CDN benchmark to title details
- Reworked speed and ETA sampling for accurate sustained-throughput readings

## 1.0.1

- Added automatic Old 3DS and Old 2DS compatibility
- Kept the dedicated Core 2 writer on New 3DS systems
- Added a safe default-core fallback instead of crashing when Core 2 is unavailable
- Updated Performance mode to show the active hardware path

## 1.0.0

- Introduced the OLED-black and light-pink Rune3DS interface
- Added custom wallpaper selection and adjustable dimming
- Added first-launch wallpaper guidance
- Redesigned title details, lists, menus, and download presentation
- Added live transfer stage, speed, ETA, size, and progress
- Added New 3DS-specific CPU, L2 cache, Core 2, buffering, and CDN optimizations
- Increased practical download throughput on tested New 3DS XL hardware
- Added quiet download cancellation
- Added custom HOME Menu banner and icon
- Hardened NBAPI parsing, authentication, HTTP status handling, and token decoding
- Added fork-safe compatibility version tracking
