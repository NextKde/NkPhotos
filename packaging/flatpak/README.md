# Flatpak / 打包

One app: `org.nextks.photos`, executable `nkphotos`. Shared runtime:
`org.kde.Platform//6.11`; build SDK: `org.kde.Sdk//6.11`. No BaseApp or
additional third-party compiled modules. Kos.Ui is statically linked; Qt and
image plugins are supplied by the runtime. Build only in the designated VM.

```sh
flatpak-builder --user --force-clean --repo=tmp/repo tmp/flatpak-build packaging/flatpak/org.nextks.photos.json
flatpak build-bundle tmp/repo tmp/org.nextks.photos.flatpak org.nextks.photos
flatpak install --user tmp/org.nextks.photos.flatpak
flatpak run org.nextks.photos
```

The first development bundle was built, installed and launched in VM 172.20.23.3.
It is approximately 1.1 MB compressed / 5 MB installed, excluding the shared
runtime. Exact checks and remaining release gates: [verification](../../docs/Verification.md).

文件夹及图片通过门户选择，不授予全盘或主目录访问、不授予网络访问。
明确点击“编辑”后，将副本经 Documents portal 授权给 `org.nextkde.nkshot`，
再通过其 KDBusService `CommandLine` 调用 `--edit-existing`。只允许此固定编辑器；
不运行 flatpak-spawn，不增加宿主辅助服务。

File/folder portals authorize browsing. The editor copy is exported persistently
so an editor can keep working after NkPhotos closes. Only that copy is granted
read/write access to the fixed NkShot identity. Missing editor and failed exports
are visible. No automatic MIME/default-app changes or catalog publication.

The NkShot checkout examined still has a legacy upstream Flatpak manifest. Its
actual released ID, D-Bus activation and document permissions must be verified
with a real `org.nextkde.nkshot` bundle before claiming cross-Flatpak integration.
No working NkShot is silently replaced with Spectacle.
