# Nocturne changelog

## 1.5.25

- Used GitHub's latest-release redirect tag as the Nocturne version source
- Removed the in-app update check's dependency on the separate `nocturne-version` release asset

## 1.5.24

- Switched the Nocturne updater back to GitHub's latest release assets
- Kept detailed startup updater diagnostics visible for failed update checks

## 1.5.23

- Accepted 3DS HTTP receive timeouts when progress shows the full known-length response has arrived
- Prevented successful HTTP cleanup from overwriting the last failure diagnostic

## 1.5.22

- Added detailed on-screen diagnostics for startup updater HTTP failures
- Showed the compiled Nocturne update base and build version in update check errors

## 1.5.21

- Stopped known-length HTTP downloads as soon as the advertised response body is complete
- Fixed tiny startup version checks that could time out after receiving the whole file

## 1.5.20

- Fixed launch update checks against tiny static `version.txt` responses on the 3DS HTTP service
- Kept HTTP receive requests bounded to the remaining response size so short version files do not time out

## 1.5.19

- Moved Nocturne's in-app updater to the Vercel static update endpoint
- Mirrored the updater CIA in the Vercel site so the 3DS no longer depends on GitHub release downloads

## 1.5.18

- Switched update metadata back to the tiny GitHub release asset now that long redirects are supported
- Avoided the GitHub Releases API response that fails on the 3DS HTTP service

## 1.5.17

- Fixed GitHub Releases API version parsing when JSON contains whitespace
- Added a visible notice if Nocturne update checking fails

## 1.5.16

- Tiny release for testing the visible in-app updater prompt and install flow

## 1.5.15

- Switched Nocturne update detection to the GitHub Releases API to avoid raw-file cache lag

## 1.5.14

- Fixed GitHub updater fetches by avoiding long release-asset redirects for version checks
- Increased HTTP redirect URL capacity for GitHub release asset downloads
- Added a visible update confirmation prompt when a Nocturne update is found

## 1.5.13

- Small polish release for validating the GitHub-based in-app updater

## 1.5.12

- Added Nocturne launch-time self-updates from GitHub Releases
- Kept official 3hs update checks non-destructive so stock CIAs do not overwrite the fork
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
- Updated the startup notice to identify Nocturne as an unofficial fork
- Clarified that official 3hs updates do not update Nocturne

## 1.1.0

- Added an optional New 3DS direct-socket CDN transport
- Kept NBAPI authentication on Nintendo's stable HTTP service
- Added automatic fallback when the direct CDN connection cannot start
- Added bounded connection timeouts and responsive download cancellation
- Added an authenticated, network-only hShop CDN benchmark to title details
- Reworked speed and ETA sampling for accurate sustained-throughput readings

## 1.0.1

- Added automatic Old 3DS and Old 2DS compatibility
- Kept the dedicated Core 2 writer on New 3DS systems
- Added a safe default-core fallback instead of crashing when Core 2 is unavailable
- Updated Performance mode to show the active hardware path

## 1.0.0

- Introduced the OLED-black and light-pink Nocturne interface
- Added custom wallpaper selection and adjustable dimming
- Added first-launch wallpaper guidance
- Redesigned title details, lists, menus, and download presentation
- Added live transfer stage, speed, ETA, size, and progress
- Added New 3DS-specific CPU, L2 cache, Core 2, buffering, and CDN optimizations
- Increased practical download throughput on tested New 3DS XL hardware
- Added quiet download cancellation
- Added custom HOME Menu banner and icon
- Hardened NBAPI parsing, authentication, HTTP status handling, and token decoding
- Added fork-safe official 3hs version tracking
