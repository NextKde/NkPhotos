# Photos interaction and visual reasoning / 照片交互与视觉推理

The target is a quiet, capable photo application. Judge each control in the
whole window and through its complete interaction, not only as an isolated widget.
目标是简单克制、功能强大。判断控件在整体中的权重，以及操作前后是否连贯。

| Situation | Intended result and reason |
| --- | --- |
| Browsing | One header, a narrow navigation column and a dense photo grid. No repeated large heading. Square crops make mixed aspect ratios scan consistently; entire-image previews and filenames remain available in View options. |
| Selection | A perimeter marks the whole photo. The footer exposes its filename without putting captions under every tile. Hover never moves or resizes tiles. |
| Open and return | Hide the library navigation for every viewer width. Keep the grid alive. Returning without changing photos restores its scroll position; after navigation, reveal the current photo. |
| Favorite | Update only the affected role outside Favorites. Never rebuild the whole grid for a heart toggle. In Favorites, removal advances to a remaining photo; removing the last returns to the empty collection. |
| Zoom and pan | Fit on each new image. +/- preserves the point under the viewport centre after panning. Opening information never resizes or recentres the image. |
| Information | A canonical anchored content popover; Escape closes information before leaving the viewer. Closing returns keyboard focus to viewing. |
| Actions | Fixed 28px canonical toolbar actions, with tooltips and keyboard access. Edit retains a text label; secondary view/appearance commands live in anchored menus. No action disappears during the mouse's journey to it. |
| Folder actions | A reserved action slot appears on row hover or keyboard focus and stays visible while its menu is open. Removing a library reference never removes files. |
| Refresh | Unchanged file lists do not reset the model. Metadata changes update existing rows when order is unchanged. A focus refresh must not reset scroll or recreate folder menus. |

图片占据主要面积；同等大小的胶囊按钮不再排成一整行。信息使用弹层，不挤动图片；
图库选择、滚动位置与看图导航保持连续。默认方形缩略图是浏览取舍，完整构图可在
查看选项中切换。显示与交互规则必须用真实窗口证据核对，不把逻辑通过称为视觉通过。

## Local library expansion / 本地图库扩展

The sidebar separates references, albums and Memories. Import means copying into
managed storage; adding a folder keeps references; opening files stays temporary.
The interface names these actions distinctly. Selection is contextual, and dialogs
freeze the selected IDs before opening. Album removal, library trash and permanent
removal have separate wording and confirmations.

侧栏区分文件夹引用、相簿和回忆。导入副本、引用目录和临时打开分别命名。
多选操作先固定目标，再打开对话框；从相簿移除、移到最近删除和永久移除不混用文案。

Date cards drill down to the existing gallery. Images and videos share selection
and organization, while their viewers show the appropriate playback or zoom tools.
Information stays in a popover. Memories use a fixed playback list and stop when
leaving; the item editor keeps ordering and cover choices explicit.

日期卡片下钻到图库；照片与视频共用整理方式，各自展示缩放或播放控件。
信息继续用弹层。回忆播放使用固定清单，退出停止；顺序和封面由用户明确选择。

Current expansion screenshots and interaction acceptance are pending; see
[Verification](Verification.md). Static source inspection does not establish visual
coordination, codec support or completed desktop handoff.

本次新版截图和交互验收尚未完成，不能以静态源码检查代替观感、编解码器和桌面交接验收。

Manual review must include both themes, Chinese and English, a small window, mixed
portrait/landscape images, an empty library, long names, popup placement, return
scroll, mouse/button navigation, keyboard focus and zoom after pan. Run only in
the designated VM. Screenshots establish sampled states; they do not prove every
gesture, codec or animation has been accepted.
