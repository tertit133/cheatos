// bookmarks.h -- the saved address list ("cheat table" entries).
//
// Each entry is a labeled address with a type and optional pinned
// value that the freezer writes back on a timer.
#pragma once

#include "common.h"
#include "value_types.h"

namespace cheat {

using BookmarkId = std::uint32_t;

struct Bookmark {
    BookmarkId  id{};
    std::string name;
    Address     address{};
    ValueType   type{ValueType::I32};
    bool        frozen{false};
    TypedValue  frozenValue;        // what the freezer writes on tick
    // Pointer path (optional). If present, the effective address is
    // resolved as *(... *(base + off[0]) ... + off[n-1]).
    std::vector<std::int32_t> pointerOffsets;
    Address     pointerBase{};
};

class BookmarkList {
public:
    BookmarkId add(Bookmark b);        // id is assigned if 0
    bool       remove(BookmarkId id);
    Bookmark*  find(BookmarkId id);
    const Bookmark* find(BookmarkId id) const;

    const std::vector<Bookmark>& list() const { return m_items; }
    std::vector<Bookmark>&       mutableList()      { return m_items; }

    void clear() { m_items.clear(); }

private:
    std::vector<Bookmark> m_items;
    BookmarkId            m_next{1};
};

} // namespace cheat
