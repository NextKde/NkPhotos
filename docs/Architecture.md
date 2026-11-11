# Architecture / 架构

NkPhotos remains one standalone Qt Quick application and one Flatpak identity,
`org.nextks.photos`. Kos.Ui supplies the shared controls, the adapted Koko ZoomArea
supplies image navigation, and NkShot owns editing. The library expansion replaces
transient folder-only organization with a persistent SQLite catalog.

一个独立应用提供图库、看图、视频和回忆播放；共享 Kos.Ui 控件与 Koko 缩放组件。
编辑仍交给 NkShot。本次将文件夹浏览模型扩展为持久化 SQLite 图库。

## Catalog and identity / 图库和身份

`LibraryStore` opens a worker-thread-owned QSQLITE connection. Its versioned schema
records media, source folders, source membership, albums, Memories, ownership and
file-operation journals. The legacy QSettings folders/favorites migrate in a
transaction, retaining unavailable and favorite-only references. Existing settings
are not erased. Media UUIDs are independent of paths and thumbnail revision tokens.

`PhotoLibrary` is the QML-facing model. It serializes work and publishes snapshots;
collection queries must not erase persistent records absent from their result.
Viewing focus, batch selection and operation snapshots are separate. Batch actions
receive captured IDs, so a later change of selection cannot retarget a pending
folder picker or confirmation. Hidden/deleted media is excluded from ordinary
collections and playback. External disappearance is an availability state.

SQLite 连接只属于工作线程；媒体 ID 不随路径或缩略图版本变化。浏览集合不删除其他集合
记录；查看焦点、多选和批量操作目标分离。隐藏和已删除内容不进入普通浏览/播放，
外部文件失联只标记不可访问。

## Reference, import and recovery / 引用、导入和恢复

Adding a folder records a nonrecursive source reference. Opening individual files
creates a temporary session. Import explicitly copies selected files to managed
storage. The data root sits next to the profile settings, including the database,
originals and recovery directory; `--profile` therefore isolates all library data.

Managed copies have recorded ownership. Trash/restore/purge and import/export use
journal records and unique destinations. Recovery does not adopt arbitrary files
that happen to occupy an operation destination. Permanent deletion is restricted
to verified managed files; external originals are never deleted. References keep
tombstones after permanent removal so refresh cannot resurrect them. Explicit
source re-addition can reintroduce references. Expiry runs while the app is open,
not from a background service.

添加文件夹仅记录位置；打开文件仅临时查看；导入明确创建副本。数据库、托管原件、恢复区
都随 profile 隔离。永久删除只处理有归属记录的托管文件；引用保留排除记录，刷新不复活。
文件事务保留中断日志，过期清理不安装后台服务。

## Media and resource bounds / 媒体与资源边界

Still images use `ImageProvider`, asynchronous scaled reads and a 48 MiB thumbnail
LRU. Preview bounds remain 4096 px, 64 MiB source files, dimensions at most 40,000 px
and 120 megapixels; decoders without scaled reading are limited to 32 megapixels.
Qt's per-read image allocation limit is 128 MiB. These image-preview limits do not
apply as a promise about video codec cost.

Qt Multimedia provides video and local music playback. Runtime `ffprobe` reads
bounded video metadata on the serialized worker. `ffmpeg` generates a first-frame
poster with bounded concurrency, output and elapsed time; no network URI is used.
Dates use plugin-readable capture metadata when available and a labeled file-date
fallback otherwise. This iteration does not add a dedicated EXIF library or person
recognition. Supported codecs depend on the deployed runtime/extensions. Folder indexing has a
20,000-file or 10-second per-refresh budget and up to 64 sources. Large sources
that exhaust that budget can leave later sources unindexed; the UI reports the
limit and asks for smaller folders. This iteration does not claim incremental
continuation through arbitrarily large libraries.

图片保留有界异步解码和缓存；视频/本地音乐使用 Qt Multimedia，信息与封面使用 runtime
已有工具。拍摄日期不可读取时明确回退文件日期，不虚构 EXIF 或人物识别能力。

## Desktop integration / 桌面交接

`EditorBridge` creates a unique editing copy and grants access through Documents
portal where required, then invokes NkShot's supported entry point. A dispatch is
not an editor-save event; the saved copy remains accessible if handoff fails.
`ShareBridge` requests the desktop's OpenFile chooser with a read-only descriptor,
explicit application selection and asynchronous response/error handling. It does
not implement automatic sending or cloud sharing.

编辑先存副本并授权交接；启动成功不等于保存完成。打开方式只交给用户所选应用，
不自动发送内容。实际 NkShot/portal 联调状态见验证文档。

## Packaging and screenshots / 打包与截图

The app reuses `org.kde.Platform//6.11`. SQL and Multimedia are runtime modules;
audio playback adds the PulseAudio socket permission. There is no new private Qt,
WebEngine, broad filesystem/network permission or resident host service.

`ScreenshotCapture` is enabled only by an explicit command-line option and requires
a separate profile plus a new absolute output path. It navigates the real loaded
window, waits for visible image loading, captures that application window and exits.
It supplies an artifact for human review, not a test harness or acceptance claim.

共用 KDE runtime，仅增加播放音频所需权限。截图开关抓取真实应用窗口，要求独立配置和
新的绝对输出路径；无自动断言，不代表运行验收通过。
