# Nocturne changelog

## Unreleased

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
