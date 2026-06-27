# Rune3DS UniStore

This folder contains the custom Universal-Updater store for Rune3DS.

Users can add this UniStore in Universal-Updater:

```text
https://raw.githubusercontent.com/p0mpurin/just-an-hshop-fork/main/universal-updater/rune3ds.unistore
```

The install script uses Universal-Updater's `downloadRelease` action to fetch
`Rune3DS.cia` from the latest non-prerelease GitHub release.

The app icon is served through `icons/rune3ds.t3x`, generated from
`icons/rune3ds.t3s`.

For each Rune3DS release:

1. Bump `storeInfo.revision`.
2. Update `storeContent[0].info.version`.
3. Update `storeContent[0].info.last_updated`.
4. Publish the matching GitHub release with the CIA asset and mark it Latest.
