#include "options.h"

#include <spdlog/fmt/bundled/core.h>
#include <spdlog/spdlog.h>
#include <utils/types.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdlib>
#include <span>
#include <string_view>

struct CliParser;

struct Entry {
  struct {
    char short_name;
    std::string_view long_name;
    std::string_view description;
  } info;

  enum class Kind { Boolean, Count, Action } kind;
  union {
    struct {
      bool *value;
      bool invert;
    } bool_entry;
    struct {
      void (*action)(CliParser &, void *);
      void *userdata;
    } action_entry;
    struct {
      std::size_t *count;
    } count_entry;
  };
};

struct CliParser {
  std::string_view program_name;
  std::string_view message;
  std::span<const Entry> entries;

  auto parse(std::span<const char *> args) -> bool;
  auto dispactch_action(const Entry &entry, bool short_arg,
                        std::string_view arg) -> bool;
};

auto CliParser::dispactch_action(const Entry &entry, bool short_arg,
                                 std::string_view arg) -> bool {
  switch (entry.kind) {
    case Entry::Kind::Boolean:
      if (entry.bool_entry.value == nullptr) {
        return false;  // MALFORMED ENTRY
      }
      *entry.bool_entry.value = !entry.bool_entry.invert;
      break;
    case Entry::Kind::Count:
      if (entry.count_entry.count == nullptr) {
        return false;  // MALFORMED ENTRY
      }
      if (short_arg) {
        const std::size_t count = arg.find_first_not_of('v', 1);
        if (count != std::string::npos) {
          return false;
        }

        *entry.count_entry.count += arg.size() - 1;
      } else {
        *entry.count_entry.count += 1;
      }
      break;

    case Entry::Kind::Action:
      if (entry.action_entry.action == nullptr) {
        return false;  // MALFORMED ENTRY
      }
      entry.action_entry.action(*this, entry.action_entry.userdata);
      break;
  }
  return true;
}

auto CliParser::parse(std::span<const char *> args) -> bool {
  for (std::string arg : args.subspan(1)) {
    bool short_arg{};
    {
      std::size_t count = arg.find_first_not_of('-');
      if (count == std::string::npos) {
        count = arg.size();
      }

      if (count == 1) {
        short_arg = true;
      } else if (count == 2) {
        short_arg = false;

      } else {
        return false;  // MALFORMED INPUT
      }
    }

    bool found = false;
    for (const auto &entry : entries) {
      bool ok = false;
      if (short_arg && entry.info.short_name != 0 && arg.size() >= 2 &&
          arg[1] == entry.info.short_name) {
        ok = true;
      }
      if (!short_arg && arg.substr(2) == entry.info.long_name) {
        ok = true;
      }

      if (!ok) {
        continue;
      }

      found = true;
      if (!dispactch_action(entry, short_arg, arg)) {
        return false;
      }
    }

    if (!found) {
      return false;  // MALFORMED INPUT
    }
  }

  return true;
}

void usage(CliParser &parser, void * /*unused*/) {
  fmt::print("Usage: {} [OPTION...]\n", parser.program_name);

  std::size_t args_space = 0;
  for (const auto &entry : parser.entries) {
    args_space = std::max(args_space, entry.info.long_name.size());
  }

  for (const auto &entry : parser.entries) {
    std::string first_part{};
    if (entry.info.short_name != 0) {
      first_part = fmt::format("-{},", entry.info.short_name);
    }
    fmt::print("  {0: ^3} --{1: <{3}} {2}\n", first_part, entry.info.long_name,
               entry.info.description, args_space);
  }
  fmt::print("{}", parser.message);
  std::exit(0);
}

auto tr::Options::from_argv(std::span<const char *> args) -> tr::Options {
  Options ret{};

  std::size_t verbose_count = 0;

  const std::array<Entry, 6> entries{
      {{
           {'h', "help", "Display this message"},
           Entry::Kind::Action,
           {.action_entry = {usage, nullptr}},
       },
       {
           {'v', "verbose", "Increase the verbosity of the ouput"},
           Entry::Kind::Count,
           {.count_entry = {&verbose_count}},
       },
       {
           {'r', "renderdoc", "Enable attach to renderdoc"},
           Entry::Kind::Boolean,
           {.bool_entry = {&ret.debug.renderdoc, false}},
       },
       {
           {0, "no-renderdoc", "Disable attach to renderdoc"},
           Entry::Kind::Boolean,
           {.bool_entry = {&ret.debug.renderdoc, true}},
       },
       {
           {'l', "validation-layers", "Enable the validation layers"},
           Entry::Kind::Boolean,
           {.bool_entry = {&ret.debug.validations_layers, false}},
       },
       {
           {0, "no-validation-layers", "Disable the validation layers"},
           Entry::Kind::Boolean,
           {.bool_entry = {&ret.debug.validations_layers, true}},
       }}};
  CliParser parser{.program_name = "ToyRenderer",
                   .message = "Done by me with love <3",
                   .entries = entries};

  if (!parser.parse(args)) {
    usage(parser, nullptr);
  }

  switch (verbose_count) {
    case 0:
      ret.debug.level = spdlog::level::info;
      break;
    case 1:
      ret.debug.level = spdlog::level::debug;
      break;
    default:
      ret.debug.level = spdlog::level::trace;
      break;
  }

  return ret;
}
