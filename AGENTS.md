# NkPhotos contributor instructions

- One application, executable `nkphotos`, Flatpak ID `org.nextks.photos`.
- Build, tests and GUI verification run only in VM 172.20.23.3 or authorized CI.
  Never build, launch, bake shaders or run tests on the maintainer workstation.
- Commit finished milestones regularly. Preserve unrelated work in other repos.
- Use pinned canonical Kos.Ui from third_party/nextkdeAppDemo. Do not fork controls.
- Gallery and viewer do not alter original files. Removing a folder removes only
  its library reference. Editing belongs to NkShot, with explicit user action.
- Use existing org.kde.Platform//6.11. No private Qt, WebEngine, SDL, or new host
  service. Any new dependency needs an explicit documented reason.
- No implicit home scanning, network content, permanent background indexing,
  automatic MIME changes or screenshot shortcut changes.
- Keep loading asynchronous, resource limits explicit, stale responses discarded,
  and paths/filenames literal. Preserve third-party copyright and licenses.
- Keep English/Chinese user interfaces and documentation aligned. Tests must use
  synthetic images and disposable profiles. No automatic verification workflows
  or public release publication without user authorization.
