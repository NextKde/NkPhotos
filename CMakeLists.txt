cmake_minimum_required(VERSION 3.21)
project(NkPhotos VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 QuickDialogs2 Concurrent DBus Sql Multimedia LinguistTools)
qt_standard_project_setup(REQUIRES 6.8)
include(GNUInstallDirs)
add_subdirectory(third_party/nextkdeAppDemo/shared/qml kos-ui)
qt_add_library(photos_core STATIC
    src/PhotoLibrary.cpp src/PhotoLibrary.h
    src/ImageProvider.cpp src/ImageProvider.h
    src/EditorBridge.cpp src/EditorBridge.h
    src/LibraryStore.cpp src/LibraryStore.h
    src/ShareBridge.cpp src/ShareBridge.h)
target_link_libraries(photos_core PUBLIC Qt6::Core Qt6::Gui Qt6::Quick Qt6::Concurrent Qt6::DBus Qt6::Sql Qt6::Multimedia)
target_include_directories(photos_core PUBLIC src)
qt_add_executable(nkphotos src/main.cpp src/Instance.h src/ScreenshotCapture.cpp src/ScreenshotCapture.h)
qt_add_library(photos_ui STATIC)
qt_add_qml_module(photos_ui URI Nk.Photos VERSION 1.0 RESOURCE_PREFIX /qt/qml
    QML_FILES qml/Main.qml qml/PhotoViewer.qml qml/ZoomArea.qml
        qml/PhotoAction.qml qml/LibraryRow.qml qml/GalleryView.qml
        qml/VideoViewer.qml qml/SlideshowControls.qml
        qml/LibrarySidebar.qml qml/DateBrowser.qml qml/CollectionBrowser.qml
        qml/LibraryDialog.qml qml/LibraryDialogs.qml qml/MemoryEditor.qml
        qml/MediaInfoPanel.qml qml/LibraryActionMenu.qml)
target_link_libraries(photos_ui PUBLIC Kos::Ui Kos::UiPlugin Qt6::QuickControls2 Qt6::QuickDialogs2)
target_link_libraries(nkphotos PRIVATE photos_core photos_ui photos_uiplugin)
qt_add_translations(nkphotos TS_FILES translations/nkphotos_zh_CN.ts RESOURCE_PREFIX /i18n)
install(TARGETS nkphotos RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES data/org.nextks.photos.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES data/org.nextks.photos.svg DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
install(FILES data/org.nextks.photos.metainfo.xml DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo)
install(FILES LICENSE NOTICE.md DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/nkphotos)
install(DIRECTORY LICENSES/ DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/nkphotos)
option(NKPHOTOS_BUILD_TESTS "Build synthetic regression tests (VM only)" OFF)
if(NKPHOTOS_BUILD_TESTS)
    enable_testing()
    find_package(Qt6 REQUIRED COMPONENTS Test)
    qt_add_executable(photos_tests tests/PhotoTests.cpp)
    target_link_libraries(photos_tests PRIVATE photos_core Qt6::Test)
    add_test(NAME photos COMMAND photos_tests)
    set_tests_properties(photos PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    qt_add_executable(photos_ui_tests tests/UiTests.cpp)
    target_link_libraries(photos_ui_tests PRIVATE photos_core photos_ui photos_uiplugin Qt6::Test)
    add_test(NAME photos_ui COMMAND photos_ui_tests)
    set_tests_properties(photos_ui PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software")
endif()
