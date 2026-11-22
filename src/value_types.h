// value_types.h -- typed scalar / string / byte-pattern values used
// across the scanner, freezer and CE-table I/O.
#pragma once

#include "common.h"

namespace cheat {

enum class ValueType {
    I8, U8, I16, U16, I32, U32, I64, U64,
    F32, F64,
    Bytes,          // "DE AD BE EF" aob-scan pattern (?? wildcard)
    AsciiString,
    WideString,
};

// Comparison a scanner applies against each candidate in memory.
// First-scan vs next-scan is a property of the scan session, not the op:
//   - Exact/Greater/Less/Between use the user-supplied operand(s).
//   - Changed/Unchanged/Increased/Decreased compare to the PREVIOUS value
//     captured by the last scan; invalid as a first-scan op.
//   - Unknown is first-scan only -- capture all values without filtering.
enum class ScanOp {
    Unknown,
    Exact,
    NotEqual,
    Greater,
    GreaterEq,
    Less,
    LessEq,
    Between,
    Changed,
    Unchanged,
    Increased,
    Decreased,
};

// Fixed byte size for integer / float types. Returns 0 for variable-length
// types (Bytes / strings).
std::size_t fixedSize(ValueType t);

bool isNumeric(ValueType t);
bool isInteger(ValueType t);
bool isFloat(ValueType t);

const char* name(ValueType t);
const char* name(ScanOp op);

struct TypedValue {
    ValueType type{ValueType::I32};
    Bytes     bytes;                    // raw storage matching `type`
    // For Bytes-scan: parallel wildcard mask, 1 = compare, 0 = wildcard.
    // Empty means no wildcards.
    Bytes     mask;

    static TypedValue zero(ValueType t);
    static TypedValue parse(ValueType t, const std::string& text,
                              bool* ok = nullptr);

    std::string toString() const;

    // Compare this value against `candidate`+`len` bytes in memory.
    // `prev` is the previously-captured bytes for next-scan ops; nullptr
    // for first-scan. `operand2` is the upper bound for Between.
    bool matches(ScanOp op,
                 const Byte* candidate, std::size_t len,
                 const Byte* prev,
                 const TypedValue* operand2) const;
};

} // namespace cheat
