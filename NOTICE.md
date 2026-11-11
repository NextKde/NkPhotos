# Source provenance

- Kos.Ui: NextKde/nextkdeAppDemo, commit
  `4aef0af171952b9c0f7da62dce199ccb51183426`, pinned git submodule. GPL-3.0;
  original LICENSE/NOTICE remain inside the dependency.
- `qml/ZoomArea.qml`: KDE/koko, commit
  `ff8a5343cd51de6984142a73b06059b28d456e43`, originally
  `src/qml/mediaview/imagedelegate/ZoomArea.qml`. Original SPDX notices retained;
  LGPL-3.0-only is the selected offered license. Changes remove Koko/Kirigami
  imports, expose enlargement as a local property, replace application-global
  single-tap behavior with a signal and correct gesture coordinate mapping.
- Forma was evaluated for product scope. No Forma UI/runtime is bundled.

NkPhotos application code is GPL-3.0-only. This is a selective component reuse,
not a complete fork of Koko or Forma. No proprietary Apple resources are used.
