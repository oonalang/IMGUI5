# Replace the whole old ImGui menu with the Neverlose one

Why tools keep failing at this: the Neverlose menu does **not** run on stock Dear ImGui.
`imgui.cpp`, `imgui_draw.cpp` and `imgui_widgets.cpp` here are patched (global `gui` colors,
`draw_blur`, `AddRectFilledMultiColorRounded`, restyled widgets). If the old ImGui folder is kept
and only the menu file is added, it will not compile or will look wrong. The ImGui folder has to
be replaced as a whole.

## Rules

1. DELETE from the old project: its ImGui folder (core + backends), its menu / UI / theme /
   widget / skin / keyboard code, its font and icon files, and every `#include` of those.
2. COPY IN, unchanged, from this repo's `app/src/main/jni/`:
   - `imgui/`  (core + `backends/imgui_impl_android.*` + `backends/imgui_impl_opengl3.*`)
   - `menu/`   (`neverlose_menu`, `hexsync_intro`, `hexsync_glow`, `gui`, `blur`, `hashes`, `color_t.hpp`, `nl_ui`, `nl_embedded_assets`)
   - `nl_layout.hpp`
   and from `app/src/main/assets/`: the 4 fonts, `avatar.rgba`, `logo.rgba`.
3. KEEP from the old project only what is not UI: its process/injection/hook entry point, its
   EGL or `eglSwapBuffers` hook, its feature logic and feature flags.
4. Call the 4 functions below from the old render loop. Nothing else of the old menu may be called.
5. Add the new `.cpp` files to the build (`Android.mk` / `CMakeLists.txt`) and delete the old ones.
   Compile with C++17 and `-DIMGUI_IMPL_OPENGL_ES3`. Link `-lGLESv3 -lEGL -llog -landroid`.

## The 4 calls

```cpp
// once, after ImGui::CreateContext(), ImGui_ImplOpenGL3_Init("#version 300 es"), GL context current
nl_ui::SetAssetReader(nl_embedded_asset_reader);   // or your own reader (see nl_ui.hpp)
nl_ui::Init(surfaceW, surfaceH);

// every frame
ImGui_ImplOpenGL3_NewFrame();
/* your platform NewFrame */
nl_ui::BeginFrame(surfaceW, surfaceH);
ImGui::NewFrame();
nl_ui::Draw();
ImGui::Render();
nl_ui::EndFrame();
ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

// touch events, screen pixel coordinates
nl_ui::OnTouch(nl_ui::Touch::Down / Move / Up, x, y);

// on teardown, GL context current
nl_ui::Shutdown();
```

Inside another process (APK assets not readable): build with `NL_EMBED=1` (defines
`NL_EMBED_ASSETS`) and use `nl_embedded_asset_reader`. For a see-through overlay call
`nl_ui::SetConfig({0.97f, 1.0f, false, false})` before `Init` (no intro, no background) and
do not clear the framebuffer.

## Connecting real features

The menu state lives in `static` variables at the top of `menu/neverlose_menu.cpp`
(toggles, sliders, combo indexes). Replace those with references to the old project's
feature flags, or read them from the old config struct. Do not add a second menu.
The Neverlose menu has no text fields, so any old keyboard code can be deleted.

## Checks before calling it done

```
grep -rn "ImGui::Begin(" --include=*.cpp --include=*.h <project>   # only neverlose_menu.cpp / hexsync_intro.cpp may match
grep -rniE "old menu names, old theme, old keyboard" <project>      # must return nothing
```
- the project contains exactly one `imgui.cpp`
- the app starts, shows the HexSync intro, then the menu with the Rage page
- rotating the device keeps the menu sharp and centered

---

## One-shot prompt for an AI coding tool

> Replace the entire ImGui UI of this project with the Neverlose/HexSync menu from
> https://github.com/oonalang/IMGUI5. Follow REPLACE_EVERYTHING.md in that repo exactly.
> Do not keep or merge the old ImGui: delete the old ImGui directory, old menu/theme/widget/
> keyboard/font files and their build entries, then copy in `app/src/main/jni/imgui`,
> `app/src/main/jni/menu`, `nl_layout.hpp` and the assets unchanged. Keep only the project's
> entry point, GL/EGL hook and feature logic, and wire the render loop to
> `nl_ui::Init / BeginFrame / Draw / EndFrame / OnTouch / Shutdown`. Update the build files,
> remove every reference to deleted files, bind the toggles/sliders in `neverlose_menu.cpp`
> to the project's existing feature flags, remove comments that add no information, and run
> the grep checks at the end of the guide. Do not write a second menu.
