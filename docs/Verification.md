# Verification / 验证

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
