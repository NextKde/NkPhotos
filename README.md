# NkPhotos

One NextKde application for photo-folder browsing and image viewing. The gallery
and single-image viewer share one executable and one Flatpak identity:
`org.nextks.photos`. Editing is delegated to NkShot; originals are not modified
by NkPhotos.

The first implementation is in progress. No release or runtime acceptance is
claimed. See [中文](README.zh-CN.md), [architecture](docs/Architecture.md), and
[verification](docs/Verification.md).

## Development

Use the pinned Kos.Ui submodule. Build and test only in the designated VM
(`172.20.23.3`) or explicitly authorized CI, never on the maintainer workstation.

```sh
git submodule update --init --recursive
cmake -S . -B build -G Ninja -DNKPHOTOS_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

Target runtime: `org.kde.Platform//6.11`; matching `org.kde.Sdk` is build-only.
No WebEngine, SDL, private Qt runtime or resident host service is planned.

## Sources

Image pan/zoom is adapted from KDE Koko. Kos.Ui is the canonical NextKde shared
component library. See [NOTICE](NOTICE.md) for exact revisions and modifications.
