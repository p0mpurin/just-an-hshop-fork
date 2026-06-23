# Nocturne changelog

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
