#pragma once

#include <string>

namespace gb {

// 1234567 -> "1,234,567"
inline std::string FormatThousands(long long value) {
    const bool negative = value < 0;
    std::string digits = std::to_string(negative ? -value : value);
    std::string out;
    const int len = static_cast<int>(digits.size());
    for (int i = 0; i < len; ++i) {
        out += digits[static_cast<std::size_t>(i)];
        const int remaining = len - i - 1;
        if (remaining > 0 && remaining % 3 == 0) {
            out += ',';
        }
    }
    return negative ? "-" + out : out;
}

} // namespace gb
