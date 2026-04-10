#pragma once

#include <cstdint>
#include <string>

class ChecksumValidator
{
public:
    // Вычисляет XOR всех байт между '$' и '*'.
    // Возвращает 0 если '$' или '*' не найдены.
    static uint8_t compute(const std::string& sentence);

    // Возвращает true если вычисленный checksum совпадает со значением
    // после '*' (регистр HEX-символов не важен).
    // Возвращает false при любом структурном нарушении (нет '$', нет '*',
    // меньше двух символов после '*', невалидные HEX-символы).
    static bool validate(const std::string& sentence);
};
