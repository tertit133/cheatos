#include "bookmarks.h"

namespace cheat {

BookmarkId BookmarkList::add(Bookmark b) {
    if (b.id == 0) b.id = m_next++;
    else           m_next = std::max(m_next, b.id + 1);
    m_items.push_back(std::move(b));
    return m_items.back().id;
}

bool BookmarkList::remove(BookmarkId id) {
    for (auto it = m_items.begin(); it != m_items.end(); ++it) {
        if (it->id == id) { m_items.erase(it); return true; }
    }
    return false;
}

Bookmark* BookmarkList::find(BookmarkId id) {
    for (auto& b : m_items) if (b.id == id) return &b;
    return nullptr;
}

const Bookmark* BookmarkList::find(BookmarkId id) const {
    for (const auto& b : m_items) if (b.id == id) return &b;
    return nullptr;
}

} // namespace cheat
