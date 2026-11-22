#include "ce_table.h"

#include <pugixml.hpp>

namespace cheat {

namespace {

const char* ceTypeName(ValueType t) {
    switch (t) {
        case ValueType::I8:  case ValueType::U8:  return "Byte";
        case ValueType::I16: case ValueType::U16: return "2 Bytes";
        case ValueType::I32: case ValueType::U32: return "4 Bytes";
        case ValueType::I64: case ValueType::U64: return "8 Bytes";
        case ValueType::F32: return "Float";
        case ValueType::F64: return "Double";
        case ValueType::AsciiString: return "String";
        case ValueType::WideString:  return "String";
        case ValueType::Bytes:       return "Array of byte";
    }
    return "4 Bytes";
}

ValueType ceTypeFromName(const std::string& s) {
    if (s == "Byte")               return ValueType::U8;
    if (s == "2 Bytes")            return ValueType::U16;
    if (s == "4 Bytes")            return ValueType::U32;
    if (s == "8 Bytes")            return ValueType::U64;
    if (s == "Float")              return ValueType::F32;
    if (s == "Double")             return ValueType::F64;
    if (s == "String")             return ValueType::AsciiString;
    if (s == "Array of byte")      return ValueType::Bytes;
    return ValueType::I32;
}

} // anonymous

bool saveCheatTable(const std::wstring& path,
                     const std::vector<Bookmark>& items) {
    pugi::xml_document doc;
    auto decl = doc.append_child(pugi::node_declaration);
    decl.append_attribute("version")  = "1.0";
    decl.append_attribute("encoding") = "utf-8";

    auto root = doc.append_child("CheatTable");
    root.append_attribute("CheatEngineTableVersion") = 31;
    auto entries = root.append_child("CheatEntries");

    for (const auto& b : items) {
        auto e = entries.append_child("CheatEntry");
        e.append_child("ID").text().set(static_cast<unsigned>(b.id));
        e.append_child("Description").text().set(("\"" + b.name + "\"").c_str());
        e.append_child("VariableType").text().set(ceTypeName(b.type));

        char addrBuf[32];
        std::snprintf(addrBuf, sizeof(addrBuf), "%llX",
                       static_cast<unsigned long long>(
                           b.pointerOffsets.empty() ? b.address : b.pointerBase));
        e.append_child("Address").text().set(addrBuf);

        if (!b.pointerOffsets.empty()) {
            auto offs = e.append_child("Offsets");
            for (auto o : b.pointerOffsets) {
                char ob[16];
                std::snprintf(ob, sizeof(ob), "%X", o);
                offs.append_child("Offset").text().set(ob);
            }
        }
    }

    return doc.save_file(path.c_str());
}

std::optional<std::vector<Bookmark>>
loadCheatTable(const std::wstring& path) {
    pugi::xml_document doc;
    auto res = doc.load_file(path.c_str());
    if (!res) return std::nullopt;

    auto root = doc.child("CheatTable");
    if (!root) return std::nullopt;

    std::vector<Bookmark> out;
    for (auto e : root.child("CheatEntries").children("CheatEntry")) {
        Bookmark b{};
        std::string desc = e.child_value("Description");
        if (desc.size() >= 2 && desc.front() == '"' && desc.back() == '"') {
            desc = desc.substr(1, desc.size() - 2);
        }
        b.name = std::move(desc);
        b.type = ceTypeFromName(e.child_value("VariableType"));

        const char* addrText = e.child_value("Address");
        b.address = static_cast<Address>(
            std::strtoull(addrText ? addrText : "0", nullptr, 16));

        if (auto offs = e.child("Offsets")) {
            for (auto o : offs.children("Offset")) {
                b.pointerOffsets.push_back(
                    static_cast<int>(std::strtol(o.child_value(), nullptr, 16)));
            }
            b.pointerBase = b.address;
            b.address     = 0;  // resolved live by the engine
        }

        b.frozenValue = TypedValue::zero(b.type);
        out.push_back(std::move(b));
    }
    return out;
}

} // namespace cheat
