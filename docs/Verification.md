# Verification / 验证

## Local-library expansion — not yet validated / 本地图库扩展尚未验证

The `feat/local-photo-library` work adds a persistent catalog, managed imports,
albums, date browsing, batch operations, hidden/recovery collections, video and
Memories playback, export, and a desktop Open With handoff. Source integration and
Chinese translations have been prepared; this is an **uncompiled development
checkpoint**, not a release or accepted build.

本次只进行了源码阅读、静态整合和依赖查询，没有运行自动化测试、应用或图形验证。
虚拟机运行时已查到 Qt 6.11.3 SQL/Multimedia、QSQLITE、FFmpeg/ffprobe；这不等于新代码编译通过。
自动审批拒绝向指定 VM 传输工作树的同步脚本，因此本次编译和新版真实效果图仍受阻。
现存截图及下面的历史记录仅对应旧版，不能作为本次验收证据。

Manual acceptance after an authorized VM build / 授权 VM 编译后的人工检查：

- Import synthetic files, restart, and confirm metadata/albums/selection behavior;
  check missing/revoked folder access without losing catalog records.
- Distinguish removing an album member, removing a folder reference, moving a
  managed copy to Recently Deleted, restoring it, and permanently removing it.
  External originals must remain in place; refresh must not resurrect tombstones.
- Export same-named files, cancel a large copy, and inspect individual results;
  retry interrupted operations without overwriting an unrelated file.
- Inspect year/month/day views, search, hidden exclusions and date fallback labels.
- Review 740×480 and normal windows, English/Chinese, light/dark, popup focus,
  Ctrl/Shift selection, return scroll and zoom after panning.
- Play representative supported/unsupported videos; seek, mute, pause and leave
  the viewer. Play a Memory with local music, reorder/remove items and exit.
- Use actual desktop pickers, Open With and NkShot copy editing on the real session.
  Private-bus screenshots do not establish these portal integrations.

Remaining implementation differences from the approved scope / 尚未完成的范围：

- Automatic date memories are now implemented for up to 12 recent eligible dates,
  with at least three photos and at most 200 photos each. They are generated once
  per date after a successful scan; deletion is respected. This remains unexecuted.
- Indexing remains nonrecursive and capped at 20,000 files or 10 seconds per refresh;
  continuation through larger sources is not implemented. Capture metadata is
  limited to available Qt plugin text and bounded video probing.
- Recovery/expiry handles at most 100 operations/items at startup; continuous
  cleanup during a long-running session is not implemented.

回忆支持主动创建，并新增有界的自动日期回顾；尚未运行验收。超大图库的连续索引和长时间
运行时的持续到期清理尚未完成。自动审批另外拒绝了每小时到期清理改动，理由为不可逆自动删除；
没有应用该改动，也没有绕过拒绝。

Application-owned screenshot support is implemented in source. No new screenshot
has been captured. Pending captures must use disposable profiles and synthetic or
redistributable media, then be linked to their exact source commit.

## Historical baseline / 历史基线

2026-11-11: first development build verified on VM **172.20.23.3**.
Code revision: `4653408`. The maintainer host was used only for source/Git and
viewing screenshots captured in the VM. No host builds, tests or application runs.

## Passed / 已通过

- Native CMake/Ninja Debug build with Qt 6.10.2 on Ubuntu 26.04.
- CTest: both targets passed. Nine core regression cases cover literal Unicode,
  spaces, percent/hash/newline paths, non-recursive enumeration, symlink skipping,
  canceled/stale scans, corrupt/changed files, decoder/file-size bounds, registered
  image access, favorites persistence, removing folder references without file
  deletion, automatic refresh after external changes, and non-overwriting copies.
- Three UI cases: English light, Chinese dark, Chinese 740×480. They load the real
  QML module, exercise gallery/viewer, keyboard navigation, zoom/fit, information,
  missing selection and return-to-gallery on folder opening. No QML warnings.
  Offscreen/software CTest passed; Wayland desktop UI cases also passed. The
  last folder-opening regression was rerun in the offscreen suite.
- Actual native executable: Wayland startup, Unicode filename, Chinese language,
  and second-process forwarding to the existing profile/window.
- Release Flatpak built against `org.kde.Sdk//6.11`, bundled and installed in
  the VM. Installed runtime is **org.kde.Platform/x86_64/6.11**, reusing the VM's
  existing platform. No extra app runtime or BaseApp.
- Installed Flatpak smoke: Wayland launch, `--file-forwarding` with a Unicode
  image, second-process activation, and denial of the original ungranted host
  pathname. Startup emitted no errors. This smoke does not replace interactive
  file-picker acceptance.
- Desktop entry and AppStream metadata validation passed (AppStream: one
  pedantic suggestion). No public release or MIME-default changes performed.

虚拟机已通过源码构建、数据与界面回归、Wayland 启动、重复打开转交已有窗口，
以及实际 Flatpak 打包、安装和启动测试。界面截图由虚拟机生成，图片均为测试素材：
[照片库](screenshots/gallery.png)、[看图](screenshots/viewer.png)。

## Build artifact / 开发包

- VM: `/home/amao/NkPhotos/tmp/org.nextks.photos.flatpak`
- Bundle: **1,097,864 bytes** (~1.1 MB); installed application: **5,004,800 bytes**
  (~5 MB), excluding shared runtime and optional debug symbols.
- Bundle SHA-256: `a5a04ba605af20f517ad355e73031fcb57145ada5142bd345cb5835870520c9b`.
- Platform commit: `fb23e178d082b117df7924f9a84710992ec233ebc007385feb85d349c4c81415`.
- Logs in VM: `NkPhotos-build.log`, `NkPhotos/build/Testing/Temporary/LastTest.log`,
  `NkPhotos/tmp/wayland-ui.log`, `NkPhotos/tmp/flatpak-build.log`.

The SDK, flatpak-builder and AppStream compose tools were added only to the VM's
build environment. The application manifest builds one module. Runtime-inherited
KDE settings access includes read-only `xdg-config/kdeglobals`; no home-directory,
whole-filesystem or network permission was added.

## Still pending before release / 发布前待验收

- Interactive real desktop file/folder picker: accept/cancel, remembered portal
  grants after restart and revocation, destination folder write access.
- A real **org.nextkde.nkshot** Flatpak: D-Bus activation, copy grants, opening,
  saving and returning to the refreshed image. NkShot was not installed in this VM;
  the inspected upstream manifest still uses Spectacle's old identity. Creating
  a copy is tested; successful editor handoff is **not** claimed.
- Actual touch/pinch/pan gestures and larger real photo collections. Tests do not
  establish a performance guarantee for every codec or hostile image.

真实文件选择器交互、权限撤销后重选和 NkShot 跨沙箱编辑尚待联调。
当前交付的是可安装的开发版；这些项目未完成前不作为正式发布验收通过。
