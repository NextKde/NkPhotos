# NkPhotos

NkPhotos is a local photo and video library for NextKde, with one application and
Flatpak identity: `org.nextks.photos`. It uses the shared KDE 6.11 runtime and
canonical Kos.Ui controls. Editing remains an explicit handoff of a copy to NkShot.

**Development status:** the local-library expansion is implemented in this branch
but has not yet been compiled or visually accepted. See [verification status](docs/Verification.md).
The images previously committed under `docs/screenshots/` show the earlier folder
browser, not this expansion. [中文](README.zh-CN.md)

## Library and organization

- Add a folder to reference its existing files, or import selected files as managed
  copies. Neither action moves the external originals. Folder scanning is local and
  nonrecursive; the app does not scan your home directory automatically.
- Browse photos and videos, date groups, favorites, recent imports, hidden items,
  albums and Recently Deleted. Dates use available capture metadata with a visible
  file-date fallback; metadata support depends on the actual decoder.
- Select with Ctrl/Shift or Select All, create and rename albums, choose a cover,
  and add/remove album members. Removing an album does not delete its media.
- Search filenames, titles, descriptions, keywords and dates. Edit descriptive
  metadata in the catalog without writing to the original files.
- View images with pan, zoom and fit; play local videos with seeking and volume.
  Unsupported codecs produce a visible error rather than a playback guarantee.
- Create local Memories from selected media or a date group, choose an order and
  cover, set photo duration, and optionally select local music. Playback offers
  pause, previous/next and fullscreen. Successful scans also generate bounded
  date-based Memories once per eligible day (three or more photos, up to 12 dates
  and 200 photos each). This is not person or location recognition.

## Files, recovery and sharing

“Open files” creates a temporary viewing session. Import those files before using
persistent organization actions. Imported copies and the SQLite catalog are held
inside the application's profile; the existing folders and favorites settings are
migrated without moving external files.

Deletion moves managed copies into the application's recovery area. Referenced
external files remain untouched. Recently Deleted retains items for 30 days;
expiry is processed when the application opens. Permanent removal of a reference
keeps an exclusion record so an ordinary refresh does not re-add it. Explicitly
adding its source again allows reintroduction. Recovery restores catalog state and
managed files where applicable. File operations are journaled for interruption
recovery and report individual errors.

Export originals copies the selected files to a chosen folder with collision-safe
names. Open With asks the desktop to choose another application; any sending or
sharing happens there. The app does not send messages or upload media itself.

Edit creates a separate, uniquely named file and opens it through
`nkshot --edit-existing` or its supported desktop activation interface. A failed
handoff retains the copy. Actual cross-Flatpak NkShot integration remains pending;
an Open With result also does not establish that a receiving app sent anything.

## Controls

- Ctrl+O: open files; Ctrl+Shift+O: add a referenced folder; Ctrl+F: search.
- Double-click or Enter: view; arrows: navigate; Escape: close the active overlay
  before returning to the library.
- Ctrl/Shift-click: batch selection; Ctrl+A: select the filtered collection.
- Wheel or +/−: image zoom; drag: pan; Fit: reset the view.
- Ctrl+B: sidebar; Ctrl+I: information; F5: refresh; F11: fullscreen.

## Development and review

Build only in the authorized VM or explicitly authorized CI. Do not build or run
this application on the maintainer workstation. Automated tests are not part of
this task; leave `NKPHOTOS_BUILD_TESTS=OFF`.

```sh
cmake -S . -B build -G Ninja -DNKPHOTOS_BUILD_TESTS=OFF
cmake --build build
```

Required Qt modules include SQL (with QSQLITE) and Multimedia. The KDE 6.11 runtime
provides those modules and FFmpeg/ffprobe used for bounded video metadata/poster
extraction. No private Qt runtime, WebEngine, network permission or host background
indexer is added. See [packaging](packaging/flatpak/README.md) and
[architecture](docs/Architecture.md).

The explicit `--screenshot=/absolute/new-file.png` option captures only the app's
window and exits. It requires `--profile=/absolute/disposable-profile`; use synthetic
media and a private session bus in the authorized VM. `--screenshot-view` chooses
a real application view (for example `gallery`, `info`, `albums`, `months` or
`selection`); views requiring media fail if no matching media exists. These images
are for human review, not automated assertions. `--appearance=light|dark` and
`--window-size=1080x720` select presentation. Do not use a personal library profile.

Image pan/zoom is adapted from KDE Koko. See [NOTICE](NOTICE.md) for attribution.
Apple's [Photos guide](https://support.apple.com/zh-cn/guide/photos/welcome/mac)
informs the local workflow; iCloud, shared cloud libraries, person recognition and
an internal adjustment editor are outside this iteration.
