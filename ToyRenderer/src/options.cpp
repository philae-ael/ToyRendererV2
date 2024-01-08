#include "options.h"

#include <spdlog/spdlog.h>
#include <utils/types.h>
#include <vulkan/vulkan_core.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdlib>
#include <format>
#include <iostream>
#include <list>
#include <map>
#include <ranges>
#include <span>
#include <string_view>

struct CliParser;

enum ParseResult {
  Ok,
  InvalidEntry,
  MalFormedInput,
  MAX,
};
const std::array<const char *, ParseResult::MAX> result_messages{
    "Ok",
    "Invalid entry",
    "Malformed input",
};

struct Entry {
  struct {
    char short_name;
    std::string_view long_name;
    std::string_view description;
    std::string_view section;
  } info;

  enum class Kind { Boolean, Count, Custom, Choice, String, None } kind = Kind::None;
  union {
    struct {
      bool *value;
      bool invert;
    } bool_entry;
    struct {
      void (*action)(CliParser &, void *);
      void *userdata;
    } custom_entry;
    struct {
      std::size_t *count;
    } count_entry;
    struct {
      std::string_view *value;
    } string_entry;
    struct {
      int *value;
      std::span<const std::pair<std::string_view, int>> choices;
    } choice_entry;
    int d{};
  };
};

struct CliParser {
  std::string_view program_name;
  std::string_view message;
  std::span<const Entry> entries;

  struct State {
    std::size_t index{};
    std::span<const char *> data;
  } state{};

  auto parse(std::span<const char *> args) -> ParseResult;
  auto parse_one(std::string_view arg) -> ParseResult;
  auto dispactch_action(const Entry &entry) -> ParseResult;

  [[nodiscard]] auto has_next() const -> bool { return state.index < state.data.size(); }
  auto next() -> std::string_view {
    auto s = state.data[state.index];
    state.index += 1;
    return s;
  }
};

auto CliParser::dispactch_action(const Entry &entry) -> ParseResult {
  switch (entry.kind) {
    case Entry::Kind::None:
      return ParseResult::InvalidEntry;
    case Entry::Kind::Boolean:
      if (entry.bool_entry.value == nullptr) {
        return ParseResult::MalFormedInput;
      }
      *entry.bool_entry.value = !entry.bool_entry.invert;
      break;
    case Entry::Kind::Count:
      if (entry.count_entry.count == nullptr) {
        return ParseResult::MalFormedInput;
      }
      *entry.count_entry.count += 1;
      break;

    case Entry::Kind::Custom:
      if (entry.custom_entry.action == nullptr) {
        return ParseResult::InvalidEntry;
      }
      entry.custom_entry.action(*this, entry.custom_entry.userdata);
      break;
    case Entry::Kind::Choice: {
      if (!has_next()) {
        return ParseResult::MalFormedInput;
      }
      auto val = next();
      auto choice = std::find_if(entry.choice_entry.choices.begin(), entry.choice_entry.choices.end(),
                                 [&](auto &v) { return v.first == val; });

      if (choice != entry.choice_entry.choices.end()) {
        *entry.choice_entry.value = choice->second;
      } else {
        return ParseResult::MalFormedInput;
      }
      break;
    }
    case Entry::Kind::String: {
      if (!has_next()) {
        return ParseResult::MalFormedInput;
      }
      *entry.string_entry.value = next();
      break;
    }
  }
  return ParseResult::Ok;
}

auto CliParser::parse_one(std::string_view arg) -> ParseResult {
  std::size_t count = arg.find_first_not_of('-');
  // For now things like " -- " are not allowed
  if (count == std::string::npos) {
    return ParseResult::MalFormedInput;
  }
  switch (count) {
    // Case 1 is buggy:
    // when Kind == choice, things like "-vpv fifo" are legal
    // while they should not be
    case 1:  // short args -h
      for (auto c : arg.substr(1)) {
        auto entry = std::find_if(entries.begin(), entries.end(), [&](auto &entry) {
          return entry.info.short_name != 0 && c == entry.info.short_name;
        });
        if (entry == entries.end()) {
          return ParseResult::MalFormedInput;
        }
        ParseResult result = dispactch_action(*entry);
        if (result != ParseResult::Ok) {
          return result;
        }
      }
      return ParseResult::Ok;
    case 2: {  // long args --help
      auto entry = std::find_if(entries.begin(), entries.end(),
                                [&](auto &entry) { return arg.substr(2) == entry.info.long_name; });

      if (entry == entries.end()) {
        return ParseResult::MalFormedInput;
      }

      return dispactch_action(*entry);
    }
    default:
      return ParseResult::MalFormedInput;
  }
}
auto CliParser::parse(std::span<const char *> args) -> ParseResult {
  state.data = args;
  state.index = 0;

  next();
  while (has_next()) {
    ParseResult result = parse_one(next());
    if (result != ParseResult::Ok) {
      return result;
    }
  }
  return ParseResult::Ok;
}

const std::size_t MAX_ARGS_SPACE = 20;

void entry_usage(const Entry *entry, std::size_t args_space) {
  std::string first_part{};
  if (entry->info.short_name != 0) {
    first_part = std::format("-{},", entry->info.short_name);
  }

  std::string description{entry->info.description};
  if (entry->info.long_name.size() > MAX_ARGS_SPACE) {
    description = std::format("\n{0: ^{1}} {2}", "", args_space + 8, description);
  }
  std::cout << std::format("  {0: ^3} --{1: <{2}} {3}", first_part, entry->info.long_name, args_space, description);

  switch (entry->kind) {
    case Entry::Kind::Count:
      std::cout << std::format(" (can be repeated)");
      break;
    case Entry::Kind::Choice: {
      std::string s;
      for (std::size_t i = 0; i < entry->choice_entry.choices.size(); i++) {
        auto &choice = entry->choice_entry.choices[i];

        s += choice.first;
        if (i != entry->choice_entry.choices.size() - 1) {
          s += ", ";
        }
      }
      std::cout << std::format(" (allowed values: {})", s);
      break;
    }
    default:
      break;
  }
}

void usage(CliParser &parser, void * /*unused*/) {
  std::cout << std::format("Usage: {} [OPTION...]\n", parser.program_name);

  std::map<std::string_view, std::list<const Entry *>> entries_per_section;

  std::size_t args_space = 0;
  for (const auto &entry : parser.entries) {
    entries_per_section[entry.info.section].push_back(&entry);

    if (args_space <= MAX_ARGS_SPACE) {
      args_space = std::min(MAX_ARGS_SPACE, std::max(args_space, entry.info.long_name.size()));
    }
  }

  for (auto &[section, entries] : entries_per_section) {
    if (!section.empty()) {
      std::cout << std::format("\n{}:\n", section);
    }

    for (const auto entry : entries) {
      entry_usage(entry, args_space);
      std::cout << std::format("\n");
    }
  }

  if (!parser.message.empty()) {
    std::cout << std::format("\n{}\n", parser.message);
  }
  std::exit(0);
}

auto tr::Options::from_args(std::span<const char *> args) -> tr::Options {
  Options ret{};

  std::size_t verbose_count = 0;

  CliParser parser{.program_name = "ToyRenderer",
                   .message = "Done by me with love <3",
                   .entries = {{
                       {
                           {'h', "help", "display this message", "Misc"},
                           Entry::Kind::Custom,
                           {.custom_entry = {usage, nullptr}},
                       },
                       {
                           {'v', "verbose", "increase the verbosity of the ouput", "Debug"},
                           Entry::Kind::Count,
                           {.count_entry = {&verbose_count}},
                       },
                       {
                           {'r', "renderdoc", "enable attach to renderdoc", "Debug"},
                           Entry::Kind::Boolean,
                           {.bool_entry = {&ret.debug.renderdoc, false}},
                       },
                       {
                           {0, "no-renderdoc", "disable attach to renderdoc", "Debug"},
                           Entry::Kind::Boolean,
                           {.bool_entry = {&ret.debug.renderdoc, true}},
                       },
                       {
                           {'l', "validation-layers", "enable the validation layers", "Debug"},
                           Entry::Kind::Boolean,
                           {.bool_entry = {&ret.debug.validations_layers, false}},
                       },
                       {
                           {0, "no-validation-layers", "disable the validation layers", "Debug"},
                           Entry::Kind::Boolean,
                           {.bool_entry = {&ret.debug.validations_layers, true}},
                       },
                       {
                           {'p', "present-mode", "chose the present-mode", "Config"},
                           Entry::Kind::Choice,
                           {.choice_entry =
                                {
                                    reinterpret_cast<int *>(&ret.config.prefered_present_mode),
                                    {{
                                        {"immediate", VK_PRESENT_MODE_IMMEDIATE_KHR},
                                        {"fifo", VK_PRESENT_MODE_FIFO_KHR},
                                        {"mailbox", VK_PRESENT_MODE_MAILBOX_KHR},
                                        {"relaxed", VK_PRESENT_MODE_FIFO_RELAXED_KHR},
                                    }},
                                }},
                       },
                       {
                           {'i', "imgui", "enable imgui", "Debug"},
                           Entry::Kind::Boolean,
                           {.bool_entry = {&ret.debug.imgui, false}},
                       },
                       {
                           {0, "no-imgui", "disable imgui", "Debug"},
                           Entry::Kind::Boolean,
                           {.bool_entry = {&ret.debug.imgui, true}},
                       },
                       {
                           {'s', "scene", "load scene", "Scene"},
                           Entry::Kind::String,
                           {.string_entry = {&ret.scene}},
                       },
                   }}};

  if (ParseResult result = parser.parse(args); result != ParseResult::Ok) {
    spdlog::warn("{}", result_messages.at(result));

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
