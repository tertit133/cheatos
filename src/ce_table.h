// ce_table.h -- read / write a subset of CheatEngine's .CT XML format.
//
// We handle the three fields that actually matter for a trainer:
// Description, VariableType, Address (hex), plus an optional
// <Offsets> chain. Advanced CE features (scripts, groupings, pointer
// scanner thresholds) are ignored on read and omitted on write.
#pragma once

#include "bookmarks.h"
#include "common.h"

namespace cheat {

bool saveCheatTable(const std::wstring& path,
                     const std::vector<Bookmark>& items);

std::optional<std::vector<Bookmark>>
loadCheatTable(const std::wstring& path);

} // namespace cheat
