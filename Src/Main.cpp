#include "Grafix.hpp"

#include <cstdio>
#include <sal.h>

_Use_decl_annotations_ int WINAPI
WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
  grafix::CreateHwnd(hInstance);
  grafix::LoadPipeline();
  grafix::LoadAssets();

  constexpr auto numFramesToRender = 200;
  for (auto i = 0; i < numFramesToRender; ++i) {
    grafix::Render();
  }

  grafix::Terminate();
}