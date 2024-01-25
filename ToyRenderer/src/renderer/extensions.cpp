#include "extensions.h"

#include <spdlog/spdlog.h>
#include <utils/assert.h>
#include <vulkan/vulkan_core.h>

#include <cstddef>
#include <tuple>

// Some magic for loading extension
// funny but unreadable
//
// the EXPAND stuff is adapted from
// https://www.scs.stanford.edu/~dm/blog/va-opt.html
//
// Quite unexpectedly, it works very well
//
// To add a function to load, juste add a
// LOAD(ExtensionFlags, nameofthefucntion, numberofarguments) and everything should be automatic
//
// load_extensions(VkInstance instance, ExtensionFlags flags) should be called before trying to call
// any function, otherwise it wont work
//
// For extension flags see extensions.h
//
// I don't think i use any UB, except for the not_loaded function
// (and it doesn't matter)
//
// playing too much in this file may crash clangd (OOM) so be careful

#define EXTENSIONS                                                     \
  LOAD(EXTENSION_FLAG_DEBUG_UTILS, vkSetDebugUtilsObjectNameEXT, 2)    \
  LOAD(EXTENSION_FLAG_DEBUG_UTILS, vkSetDebugUtilsObjectTagEXT, 2)     \
  LOAD(EXTENSION_FLAG_DEBUG_UTILS, vkQueueBeginDebugUtilsLabelEXT, 2)  \
  LOAD(EXTENSION_FLAG_DEBUG_UTILS, vkQueueEndDebugUtilsLabelEXT, 1)    \
  LOAD(EXTENSION_FLAG_DEBUG_UTILS, vkQueueInsertDebugUtilsLabelEXT, 2) \
  LOAD(EXTENSION_FLAG_DEBUG_UTILS, vkCmdBeginDebugUtilsLabelEXT, 2)    \
  LOAD(EXTENSION_FLAG_DEBUG_UTILS, vkCmdEndDebugUtilsLabelEXT, 1)      \
  LOAD(EXTENSION_FLAG_DEBUG_UTILS, vkCmdInsertDebugUtilsLabelEXT, 2)   \
  LOAD(EXTENSION_FLAG_DEBUG_UTILS, vkCreateDebugUtilsMessengerEXT, 4)  \
  LOAD(EXTENSION_FLAG_DEBUG_UTILS, vkDestroyDebugUtilsMessengerEXT, 3) \
  LOAD(EXTENSION_FLAG_DEBUG_UTILS, vkSubmitDebugUtilsMessageEXT, 4)
// NOLINTBEGIN

#define EVAL(a) a

namespace {
#define LOAD(section, name, arg_count) PFN_##name pfn_##name = (PFN_##name)not_loaded;

// This is probably UB, but it shoudl work on the major platforms
void not_loaded() { spdlog::warn("extension has not been setup, has load_extensions been called?"); }

void nop() {}

EVAL(EXTENSIONS)

#undef LOAD
}  // namespace

#define LOAD(section, name, arg_count)                                                            \
  do {                                                                                            \
    if ((flags & (ExtensionFlags::section)) == (ExtensionFlags::section)) {                       \
      auto tmp_pfn_##name = reinterpret_cast<PFN_##name>(vkGetInstanceProcAddr(instance, #name)); \
      if (tmp_pfn_##name == nullptr) {                                                            \
        spdlog::debug("could not load function " #name);                                          \
        pfn_##name = (PFN_##name)nop;                                                             \
      } else {                                                                                    \
        pfn_##name = tmp_pfn_##name;                                                              \
      }                                                                                           \
    }                                                                                             \
  } while (0);

void load_extensions(VkInstance instance, ExtensionFlags flags) { EVAL(EXTENSIONS) }
#undef LOAD

template <const size_t N, class F>
struct info_type;

template <const size_t N, class R, class... A>
struct info_type<N, R (*)(A...)> {
  using return_t = R;

  using arg_t = std::tuple_element_t<N, std::tuple<A...>>;
};

template <class F>
using return_t = info_type<0, F>::return_t;

template <const size_t N, class F>
using arg_t = info_type<N, F>::arg_t;

#define PARENS ()
// Similar to eval but way more times
#define EXPAND(...) EXPAND4(EXPAND4(EXPAND4(EXPAND4(__VA_ARGS__))))
#define EXPAND4(...) EXPAND3(EXPAND3(EXPAND3(EXPAND3(__VA_ARGS__))))
#define EXPAND3(...) EXPAND2(EXPAND2(EXPAND2(EXPAND2(__VA_ARGS__))))
#define EXPAND2(...) EXPAND1(EXPAND1(EXPAND1(EXPAND1(__VA_ARGS__))))
#define EXPAND1(...) __VA_ARGS__

// To count the number of argumnents
#define M10() M9
#define M9() M8
#define M8() M7
#define M7() M6
#define M6() M5
#define M5() M4
#define M4() M3
#define M3() M2
#define M2() M1
#define M1() M0
#define M0()

// To generate arg names
#define g10() g11
#define g9() g10
#define g8() g9
#define g7() g8
#define g6() g7
#define g5() g6
#define g4() g5
#define g3() g4
#define g2() g3
#define g1() g2
#define g0() g1

#define ARG(g) ar##g

#define LOAD(section, name, N) \
  return_t<PFN_##name> name(ARG_LIST(PFN_##name, 0, g##0, M##N())) { return pfn_##name(CALL_LIST(g##0, M##N())); }

#define ARG_LIST(name, i, M, ...) __VA_OPT__(ARG_LIST_HELPER(name, i, M, __VA_ARGS__()))
#define ARG_LIST_AGAIN() ARG_LIST_HELPER
#define ARG_LIST_HELPER(name, i, M, ...) \
  arg_t<i, name> ARG(M) __VA_OPT__(, ARG_LIST_AGAIN PARENS(name, i + 1, M PARENS, __VA_ARGS__ PARENS))

#define CALL_LIST(M, ...) __VA_OPT__(CALL_LIST_HELPER(M, __VA_ARGS__()))
#define CALL_LIST_AGAIN() CALL_LIST_HELPER
#define CALL_LIST_HELPER(M, ...) \
  ARG(M)                         \
  __VA_OPT__(, CALL_LIST_AGAIN PARENS(M PARENS, __VA_ARGS__ PARENS))

EXPAND(EXTENSIONS)

#undef LOAD
// NOLINTEND
