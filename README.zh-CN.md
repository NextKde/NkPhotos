# NkPhotos

照片文件夹浏览与单图查看合为一个 NextKde 应用、一个 Flatpak 包：
`org.nextks.photos`。编辑交给 NkShot，NkPhotos 不修改原图。

第一版正在实现，尚未发布或完成运行验收。只添加用户明确选择的文件夹；
不自动扫描主目录，不复制原图进入私有图库，不递归导入子目录。

使用固定版本 Kos.Ui；缩放交互改编自 KDE Koko。来源见 [NOTICE](NOTICE.md)。
共用 `org.kde.Platform//6.11`；不引入 WebEngine、SDL、自建 Qt runtime 或宿主常驻服务。

构建与验证只在 `172.20.23.3` 虚拟机或明确授权的 CI 执行。本机只编辑源码和操作 Git。
构建命令见 [English](README.md)，验收状态见 [验证记录](docs/Verification.md)。
