#include "parser/ChecksumValidator.h"

#include <cctype>

uint8_t ChecksumValidator::compute(const std::string& sentence)
{
    const auto dollar = sentence.find('$');
    const auto star   = sentence.find('*');

    if (dollar == std::string::npos || star == std::string::npos || star <= dollar)
        return 0;

    uint8_t cs = 0;
    for (std::size_t i = dollar + 1; i < star; ++i)
        cs ^= static_cast<uint8_t>(sentence[i]);

    return cs;
}

bool ChecksumValidator::validate(const std::string& sentence)
{
    const auto star = sentence.find('*');

    // Нужны: '$' где-то до '*', и ровно два символа после '*'
    if (star == std::string::npos)
        return false;

    if (sentence.find('$') == std::string::npos)
        return false;

    if (star + 2 >= sentence.size())   // нет двух символов после '*'
        return false;

    const char hi = static_cast<char>(std::toupper(static_cast<unsigned char>(sentence[star + 1])));
    const char lo = static_cast<char>(std::toupper(static_cast<unsigned char>(sentence[star + 2])));

    if (!std::isxdigit(static_cast<unsigned char>(hi)) ||
        !std::isxdigit(static_cast<unsigned char>(lo)))
        return false;

    auto hexval = [](char c) -> uint8_t {
        if (c >= '0' && c <= '9') return static_cast<uint8_t>(c - '0');
        return static_cast<uint8_t>(c - 'A' + 10);
    };

    const uint8_t expected = static_cast<uint8_t>((hexval(hi) << 4) | hexval(lo));
    return compute(sentence) == expected;
}
