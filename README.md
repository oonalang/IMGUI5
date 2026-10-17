# Neverlose / HexSync ImGui menu for Android (NDK + AIDE Pro)

Port of `neverlose-last-main` (Win32 + DirectX 11) to a NativeActivity app using
EGL + OpenGL ES 3. The menu code is the same ImGui draw-list code as the original.

## Layout

```
app/src/main/
  AndroidManifest.xml
  java/com/neverlose/menu/MainActivity.java   tiny NativeActivity subclass (immersive mode)
  assets/                                     fonts + avatar.rgba + logo.rgba
  jni/
    Android.mk, Application.mk
    main.cpp            EGL setup, fonts, textures, touch input, frame loop
    nl_layout.hpp       fits the 750x756 canvas to the screen, scales draw data and touch
    imgui/              your modified ImGui 1.89.7 core + android/opengl3 backends
    menu/               neverlose_menu, hexsync_intro, gui, blur stub, hashes
```

## Build in AIDE Pro

1. Copy the `NeverloseAndroid` folder to the phone and open it in AIDE Pro.
2. In AIDE: Settings > install the NDK when it asks (needs `android/native_app_glue`).
3. Build. AIDE runs `ndk-build` on `app/src/main/jni/Android.mk`
   (`Application.mk`: arm64-v8a + armeabi-v7a, android-21, c++_static, C++17).
4. Install and run.

Android Studio also works: `app/build.gradle` points `externalNativeBuild` at the same `Android.mk`.
Adjust the Gradle plugin version in the top-level `build.gradle` to whatever your tool uses.

## What changed from the Windows version

| Windows | Android |
|---|---|
| `ID3D11ShaderResourceView*` | `ImTextureID` |
| D3DX texture loading | `avatar.rgba` / `logo.rgba` (raw RGBA, 8 byte header), no image decoder needed |
| Fonts from files | Fonts from APK assets via `AAssetManager` |
| `blur.cpp` (DX11 gaussian blur of the wallpaper) | no-op stub; the shell's translucent fill sits on a gradient background |
| PingFang SC (Chinese glyphs, 11 MB) | dropped |
| Mouse | Touch, converted from screen pixels to the logical canvas |
| Fixed 750x756 window | Same logical canvas, scaled to fit any screen and re-fitted on rotation |

ImGui core edits that were needed: removed `#include <windows.h>` from `imgui_widgets.cpp`
and turned the `examples/example_win32_directx11/...` includes into plain includes
(`gui.hpp`, `blur.hpp`, `hashes.hpp` live in `jni/menu/`).

## Tuning

At the top of `main.cpp`: `kFill` (share of the screen used), `kUserScale`,
`kPlayIntro`, `kDrawBackground`.

## Using the menu inside your own overlay

Keep `menu/`, `nl_layout.hpp` and the font setup from `AddFontsAtScale()`. Each frame:

```cpp
auto layout = nl::Fit(surfaceW, surfaceH);
io.DisplaySize = ImVec2(nl::kCanvasW, nl::kCanvasH);   // before NewFrame
ImGui::NewFrame();
RenderNeverloseMenu(avatarTexture, 1.0f);
ImGui::Render();
nl::TransformDrawData(ImGui::GetDrawData(), layout);    // then your normal RenderDrawData
// touch: ImVec2 p = nl::ToLogical(layout, x, y); io.AddMousePosEvent(p.x, p.y);
```

## Notes

- The controls are the original's demo state; nothing is wired to real features yet.
- Not tested on a device. What was checked: the menu, intro and ImGui core compile and
  run headless on desktop (all pages render), and `main.cpp` passes a syntax check
  against stub NDK headers. First real build may need small fixes for your NDK version.
