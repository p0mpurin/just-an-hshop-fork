# Nocturne UniStore

This folder contains the custom Universal-Updater store for Nocturne.

Users can add this UniStore in Universal-Updater:

```text
https://raw.githubusercontent.com/p0mpurin/just-an-hshop-fork/main/universal-updater/nocturne.unistore
```

The install script uses Universal-Updater's `downloadRelease` action to fetch
`3hs.cia` from the latest non-prerelease GitHub release in
`p0mpurin/just-an-hshop-fork`.

The app icon is served through `icons/nocturne.t3x`, generated from
`icons/nocturne.t3s`.

For each Nocturne release:

1. Bump `storeInfo.revision`.
2. Update `storeContent[0].info.version`.
3. Update `storeContent[0].info.last_updated`.
4. Publish the matching GitHub release with a `3hs.cia` asset and mark it Latest.
