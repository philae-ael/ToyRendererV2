#include "debug.h"

#include <spdlog/spdlog.h>
#include <utils/assert.h>
#include <utils/misc.h>

#if defined(_WIN32)
#else
#include <dlfcn.h>
#endif

auto tr::renderer::Renderdoc::init() -> Renderdoc {
  Renderdoc renderdoc{};

#if defined(_WIN32)
#else
  if (void* mod = dlopen("librenderdoc.so", RTLD_NOW); mod != nullptr) {
    auto RENDERDOC_GetAPI = reinterpret_cast<pRENDERDOC_GetAPI>(dlsym(mod, "RENDERDOC_GetAPI"));
    int ret = RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_1_2, reinterpret_cast<void**>(&renderdoc.rdoc_api));

    TR_ASSERT(ret == 1 && renderdoc.rdoc_api != nullptr, "Could not load renderdoc");
    spdlog::info("Renderdoc loaded!");
  } else {
    spdlog::info("Can't find renderdoc");
  }
#endif

  return renderdoc;
}
