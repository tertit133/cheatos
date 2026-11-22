#include "value_types.h"

#include <charconv>
#include <cwchar>
#include <sstream>

namespace cheat {

std::size_t fixedSize(ValueType t) {
    switch (t) {
        case ValueType::I8:  case ValueType::U8:  return 1;
        case ValueType::I16: case ValueType::U16: return 2;
        case ValueType::I32: case ValueType::U32: case ValueType::F32: return 4;
        case ValueType::I64: case ValueType::U64: case ValueType::F64: return 8;
        default: return 0;   // Bytes / strings are variable
    }
}

bool isInteger(ValueType t) {
    switch (t) {
        case ValueType::I8:  case ValueType::U8:
        case ValueType::I16: case ValueType::U16:
        case ValueType::I32: case ValueType::U32:
        case ValueType::I64: case ValueType::U64:
            return true;
        default: return false;
    }
}

bool isFloat(ValueType t) {
    return t == ValueType::F32 || t == ValueType::F64;
}

bool isNumeric(ValueType t) { return isInteger(t) || isFloat(t); }

const char* name(ValueType t) {
    switch (t) {
        case ValueType::I8:  return "int8";
        case ValueType::U8:  return "uint8";
        case ValueType::I16: return "int16";
        case ValueType::U16: return "uint16";
        case ValueType::I32: return "int32";
        case ValueType::U32: return "uint32";
        case ValueType::I64: return "int64";
        case ValueType::U64: return "uint64";
        case ValueType::F32: return "float";
        case ValueType::F64: return "double";
        case ValueType::Bytes:       return "bytes";
        case ValueType::AsciiString: return "string";
        case ValueType::WideString:  return "wstring";
    }
    return "?";
}

const char* name(ScanOp op) {
    switch (op) {
        case ScanOp::Unknown:    return "unknown";
        case ScanOp::Exact:      return "==";
        case ScanOp::NotEqual:   return "!=";
        case ScanOp::Greater:    return ">";
        case ScanOp::GreaterEq:  return ">=";
        case ScanOp::Less:       return "<";
        case ScanOp::LessEq:     return "<=";
        case ScanOp::Between:    return "between";
        case ScanOp::Changed:    return "changed";
        case ScanOp::Unchanged:  return "unchanged";
        case ScanOp::Increased:  return "increased";
        case ScanOp::Decreased:  return "decreased";
    }
    return "?";
}

TypedValue TypedValue::zero(ValueType t) {
    TypedValue v;
    v.type = t;
    std::size_t sz = fixedSize(t);
    if (sz) v.bytes.assign(sz, 0);
    return v;
}

namespace {

template <typename T>
TypedValue packInt(ValueType vt, long long x) {
    TypedValue v;
    v.type = vt;
    T n = static_cast<T>(x);
    v.bytes.assign(reinterpret_cast<Byte*>(&n),
                    reinterpret_cast<Byte*>(&n) + sizeof(T));
    return v;
}

template <typename T>
TypedValue packFloat(ValueType vt, double x) {
    TypedValue v;
    v.type = vt;
    T n = static_cast<T>(x);
    v.bytes.assign(reinterpret_cast<Byte*>(&n),
                    reinterpret_cast<Byte*>(&n) + sizeof(T));
    return v;
}

// Parse "DE AD BE EF" or "DE ?? BE EF" style aob patterns.
std::pair<Bytes, Bytes> parseAob(const std::string& text) {
    Bytes out, mask;
    const char* p = text.c_str();
    while (*p) {
        while (*p == ' ' || *p == '\t' || *p == ',') ++p;
        if (!*p) break;
        if (p[0] == '?' && p[1] == '?') {
            out.push_back(0);
            mask.push_back(0);
            p += 2;
        } else if (std::isxdigit(static_cast<unsigned char>(p[0])) &&
                   std::isxdigit(static_cast<unsigned char>(p[1]))) {
            char buf[3] = { p[0], p[1], 0 };
            out.push_back(static_cast<Byte>(std::strtoul(buf, nullptr, 16)));
            mask.push_back(1);
            p += 2;
        } else {
            return {{}, {}};  // malformed
        }
    }
    return { std::move(out), std::move(mask) };
}

} // anonymous

TypedValue TypedValue::parse(ValueType t, const std::string& text, bool* ok) {
    auto setOk = [&](bool v) { if (ok) *ok = v; };
    setOk(true);

    if (isInteger(t)) {
        try {
            long long x = 0;
            std::size_t pos = 0;
            if (text.rfind("0x", 0) == 0 || text.rfind("0X", 0) == 0) {
                x = std::stoll(text.substr(2), &pos, 16);
            } else {
                x = std::stoll(text, &pos, 10);
            }
            switch (t) {
                case ValueType::I8:  return packInt<std::int8_t>(t, x);
                case ValueType::U8:  return packInt<std::uint8_t>(t, x);
                case ValueType::I16: return packInt<std::int16_t>(t, x);
                case ValueType::U16: return packInt<std::uint16_t>(t, x);
                case ValueType::I32: return packInt<std::int32_t>(t, x);
                case ValueType::U32: return packInt<std::uint32_t>(t, x);
                case ValueType::I64: return packInt<std::int64_t>(t, x);
                case ValueType::U64: return packInt<std::uint64_t>(t, x);
                default: break;
            }
        } catch (...) { setOk(false); return zero(t); }
    } else if (isFloat(t)) {
        try {
            double x = std::stod(text);
            return (t == ValueType::F32) ? packFloat<float>(t, x)
                                          : packFloat<double>(t, x);
        } catch (...) { setOk(false); return zero(t); }
    } else if (t == ValueType::AsciiString) {
        TypedValue v;
        v.type = t;
        v.bytes.assign(text.begin(), text.end());
        return v;
    } else if (t == ValueType::WideString) {
        TypedValue v;
        v.type = t;
        std::wstring w = utf8_to_wide(text);
        v.bytes.assign(reinterpret_cast<const Byte*>(w.data()),
                        reinterpret_cast<const Byte*>(w.data() + w.size()));
        return v;
    } else if (t == ValueType::Bytes) {
        TypedValue v;
        v.type = t;
        auto [bs, ms] = parseAob(text);
        if (bs.empty()) { setOk(false); return v; }
        v.bytes = std::move(bs);
        v.mask  = std::move(ms);
        return v;
    }

    setOk(false);
    return zero(t);
}

std::string TypedValue::toString() const {
    auto u8  = [&](std::size_t i) { return bytes[i]; };
    auto rd  = [&](auto* p, std::size_t n) {
        if (bytes.size() < n) return false;
        std::memcpy(p, bytes.data(), n);
        return true;
    };
    char buf[64];
    switch (type) {
        case ValueType::I8:  return sfmt("%d",   (int)(std::int8_t)u8(0));
        case ValueType::U8:  return sfmt("%u",   (unsigned)u8(0));
        case ValueType::I16: { std::int16_t v;  rd(&v, 2); return sfmt("%d",  (int)v); }
        case ValueType::U16: { std::uint16_t v; rd(&v, 2); return sfmt("%u",  (unsigned)v); }
        case ValueType::I32: { std::int32_t v;  rd(&v, 4); return sfmt("%d",  (int)v); }
        case ValueType::U32: { std::uint32_t v; rd(&v, 4); return sfmt("%u",  (unsigned)v); }
        case ValueType::I64: { std::int64_t v;  rd(&v, 8); return sfmt("%lld",(long long)v); }
        case ValueType::U64: { std::uint64_t v; rd(&v, 8); return sfmt("%llu",(unsigned long long)v); }
        case ValueType::F32: { float v;         rd(&v, 4); std::snprintf(buf,sizeof(buf),"%g",v); return buf; }
        case ValueType::F64: { double v;        rd(&v, 8); std::snprintf(buf,sizeof(buf),"%g",v); return buf; }
        case ValueType::AsciiString:
            return std::string(bytes.begin(), bytes.end());
        case ValueType::WideString: {
            std::wstring w(reinterpret_cast<const wchar_t*>(bytes.data()),
                            bytes.size() / sizeof(wchar_t));
            return wide_to_utf8(w);
        }
        case ValueType::Bytes: {
            std::string s;
            for (std::size_t i = 0; i < bytes.size(); ++i) {
                if (!s.empty()) s += ' ';
                if (i < mask.size() && mask[i] == 0) s += "??";
                else s += sfmt("%02X", bytes[i]);
            }
            return s;
        }
    }
    return {};
}

namespace {

template <typename T>
bool cmpInt(ScanOp op, const Byte* a, const Byte* b, const Byte* prev) {
    T x, y;
    std::memcpy(&x, a, sizeof(T));
    std::memcpy(&y, b, sizeof(T));
    T p = 0;
    if (prev) std::memcpy(&p, prev, sizeof(T));
    switch (op) {
        case ScanOp::Exact:     return x == y;
        case ScanOp::NotEqual:  return x != y;
        case ScanOp::Greater:   return x >  y;
        case ScanOp::GreaterEq: return x >= y;
        case ScanOp::Less:      return x <  y;
        case ScanOp::LessEq:    return x <= y;
        case ScanOp::Changed:   return x != p;
        case ScanOp::Unchanged: return x == p;
        case ScanOp::Increased: return x >  p;
        case ScanOp::Decreased: return x <  p;
        default: return false;
    }
}

template <typename T>
bool cmpBetween(const Byte* a, const Byte* lo, const Byte* hi) {
    T x, l, h;
    std::memcpy(&x, a,  sizeof(T));
    std::memcpy(&l, lo, sizeof(T));
    std::memcpy(&h, hi, sizeof(T));
    if (l > h) std::swap(l, h);
    return x >= l && x <= h;
}

template <typename T>
bool cmpFloat(ScanOp op, const Byte* a, const Byte* b, const Byte* prev) {
    T x, y;
    std::memcpy(&x, a, sizeof(T));
    std::memcpy(&y, b, sizeof(T));
    T p = 0;
    if (prev) std::memcpy(&p, prev, sizeof(T));
    const T eps = static_cast<T>(0.0001);
    switch (op) {
        case ScanOp::Exact:     return std::abs(x - y) < eps;
        case ScanOp::NotEqual:  return std::abs(x - y) >= eps;
        case ScanOp::Greater:   return x >  y;
        case ScanOp::GreaterEq: return x >= y;
        case ScanOp::Less:      return x <  y;
        case ScanOp::LessEq:    return x <= y;
        case ScanOp::Changed:   return std::abs(x - p) >= eps;
        case ScanOp::Unchanged: return std::abs(x - p) <  eps;
        case ScanOp::Increased: return x >  p;
        case ScanOp::Decreased: return x <  p;
        default: return false;
    }
}

template <typename T>
bool cmpFloatBetween(const Byte* a, const Byte* lo, const Byte* hi) {
    T x, l, h;
    std::memcpy(&x, a,  sizeof(T));
    std::memcpy(&l, lo, sizeof(T));
    std::memcpy(&h, hi, sizeof(T));
    if (l > h) std::swap(l, h);
    return x >= l && x <= h;
}

} // anonymous

bool TypedValue::matches(ScanOp op,
                          const Byte* candidate, std::size_t len,
                          const Byte* prev,
                          const TypedValue* operand2) const {
    if (op == ScanOp::Unknown) return true;

    std::size_t need = fixedSize(type);
    if (need > 0 && len < need) return false;

    if (type == ValueType::Bytes) {
        if (bytes.size() > len) return false;
        for (std::size_t i = 0; i < bytes.size(); ++i) {
            if (!mask.empty() && mask[i] == 0) continue;
            if (candidate[i] != bytes[i]) return false;
        }
        return op == ScanOp::Exact;   // Bytes only supports Exact today
    }

    if (type == ValueType::AsciiString || type == ValueType::WideString) {
        if (bytes.size() > len) return false;
        if (op != ScanOp::Exact) return false;
        return std::memcmp(candidate, bytes.data(), bytes.size()) == 0;
    }

    if (op == ScanOp::Between) {
        if (!operand2 || operand2->bytes.size() < need) return false;
        switch (type) {
            case ValueType::I8:  return cmpBetween<std::int8_t>  (candidate, bytes.data(), operand2->bytes.data());
            case ValueType::U8:  return cmpBetween<std::uint8_t> (candidate, bytes.data(), operand2->bytes.data());
            case ValueType::I16: return cmpBetween<std::int16_t> (candidate, bytes.data(), operand2->bytes.data());
            case ValueType::U16: return cmpBetween<std::uint16_t>(candidate, bytes.data(), operand2->bytes.data());
            case ValueType::I32: return cmpBetween<std::int32_t> (candidate, bytes.data(), operand2->bytes.data());
            case ValueType::U32: return cmpBetween<std::uint32_t>(candidate, bytes.data(), operand2->bytes.data());
            case ValueType::I64: return cmpBetween<std::int64_t> (candidate, bytes.data(), operand2->bytes.data());
            case ValueType::U64: return cmpBetween<std::uint64_t>(candidate, bytes.data(), operand2->bytes.data());
            case ValueType::F32: return cmpFloatBetween<float>   (candidate, bytes.data(), operand2->bytes.data());
            case ValueType::F64: return cmpFloatBetween<double>  (candidate, bytes.data(), operand2->bytes.data());
            default: return false;
        }
    }

    switch (type) {
        case ValueType::I8:  return cmpInt<std::int8_t>  (op, candidate, bytes.data(), prev);
        case ValueType::U8:  return cmpInt<std::uint8_t> (op, candidate, bytes.data(), prev);
        case ValueType::I16: return cmpInt<std::int16_t> (op, candidate, bytes.data(), prev);
        case ValueType::U16: return cmpInt<std::uint16_t>(op, candidate, bytes.data(), prev);
        case ValueType::I32: return cmpInt<std::int32_t> (op, candidate, bytes.data(), prev);
        case ValueType::U32: return cmpInt<std::uint32_t>(op, candidate, bytes.data(), prev);
        case ValueType::I64: return cmpInt<std::int64_t> (op, candidate, bytes.data(), prev);
        case ValueType::U64: return cmpInt<std::uint64_t>(op, candidate, bytes.data(), prev);
        case ValueType::F32: return cmpFloat<float>      (op, candidate, bytes.data(), prev);
        case ValueType::F64: return cmpFloat<double>     (op, candidate, bytes.data(), prev);
        default: return false;
    }
}

} // namespace cheat
