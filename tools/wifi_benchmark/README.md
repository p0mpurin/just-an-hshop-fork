# Nocturne Wi-Fi Lab

A small network-only throughput test for Nintendo 3DS systems.

- **A:** downloads 32 MiB using one HTTPS connection.
- **X:** downloads 32 MiB using two simultaneous HTTPS connections.
- Received data stays in RAM and is discarded. Nothing is written to the SD card.

Compare the reported average with Nocturne's normal install speed:

- A much higher benchmark result suggests the SD/AM installation path is limiting downloads.
- A similar result suggests Wi-Fi, HTTP processing, or the remote connection is the limit.
- If dual connection is meaningfully faster than single connection, a range-based multi-connection experiment may be worthwhile.

The test currently requests byte ranges from Tele2's public speed-test file over HTTP. Results therefore measure the console's path to Tele2, not hShop's CDN itself.
