# Architecture / 架构

One standalone Qt Quick application has two views: folder-photo grid and image
viewer. User-selected folders are library references, not imported file copies.
Favorites reference file URLs. Session file selections remain separate from the
remembered folders. Browsing is local, non-recursive and read-only.

一个应用提供照片网格和看图视图。用户选择的文件夹仅记录位置，不搬移文件；
收藏记录文件 URL。临时打开的文件与长期文件夹分开。浏览本地目录、不递归、不改原图。

Qt owns image decoding; a bounded provider supplies asynchronous scaled frames.
Koko provides the adapted zoom/pan component. Kos.Ui owns shared visual controls.
Application adapters own directory enumeration, settings and editor handoff.

Qt 负责解码与缩略图，Koko 缩放组件负责查看交互，Kos.Ui 负责共享控件；
目录枚举、偏好和编辑器交接由本应用管理。

Both views ship in `org.nextks.photos` using `org.kde.Platform//6.11`.
NkShot is optional and independently installed. An absent editor must produce a
visible actionable message; never invoke Spectacle as an implicit fallback.

两个视图共用同一个 Flatpak 身份和 Qt runtime。NkShot 为可选独立应用；缺失时明确提示，
不偷偷调用 Spectacle。标准 Flatpak OpenURI cannot select a named editor, and the
current NkShot desktop entry has no image MIME handler. Its verified CLI entry is
`nkshot --edit-existing FILE`. Cross-sandbox handoff needs an explicit document
grant plus an existing supported activation interface; raw private paths are not
a valid cross-sandbox contract.

## Resource bounds / 资源边界

The directory worker is serialized and cancels stale scans. The visible model
contains at most 20,000 files from at most 64 selected folders. Thumbnail decoding
is asynchronous (384 px, 48 MiB LRU); full previews are bounded to 4096 px.
Files above 64 MiB, dimensions above 40,000 px or 120 megapixels are rejected;
decoders without scaled reading are additionally limited to 32 megapixels.
Qt's per-read image allocation limit is 128 MiB. These are preview limits, not
an assurance that every image format has identical decoding cost. Animated images
are shown as a still frame; this is not a full-resolution proofing or RAW editor.

后台目录扫描串行运行，过期请求会取消；最多记录 64 个文件夹、显示 2 万张图片。
缩略图缓存、解码大小和预览分辨率有明确上限。动画仅显示静态帧，不提供 RAW
处理或全分辨率校样。格式由实际 Qt runtime 的图像插件决定。

Preferences use QSettings with a profile lock and D-Bus single-instance forwarding.
Folder and selected-file watches refresh external changes; focus and F5 also
refresh. No background database or system indexer is installed. The application
does not add a second editor, and does not infer an editor-completed event from a
successful command dispatch.
