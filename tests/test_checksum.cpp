#include <gtest/gtest.h>

#include "parser/ChecksumValidator.h"

// ---------------------------------------------------------------------------
// compute() — XOR всех байт между '$' и '*'
// ---------------------------------------------------------------------------

TEST(ChecksumValidatorTest, ComputeKnownRmcSentence)
{
    // $GPRMC,120000,A,5545.1234,N,03739.5678,E,025.0,045.0,270124,,,A*70
    // Эталон рассчитан вручную (PowerShell XOR)
    EXPECT_EQ(ChecksumValidator::compute(
        "$GPRMC,120000,A,5545.1234,N,03739.5678,E,025.0,045.0,270124,,,A*70"),
        0x70u);
}

TEST(ChecksumValidatorTest, ComputeKnownGgaSentence)
{
    // $GPGGA,120000,5545.1234,N,03739.5678,E,1,10,0.8,150.0,M,14.0,M,,*4E
    EXPECT_EQ(ChecksumValidator::compute(
        "$GPGGA,120000,5545.1234,N,03739.5678,E,1,10,0.8,150.0,M,14.0,M,,*4E"),
        0x4Eu);
}

TEST(ChecksumValidatorTest, ComputeShortSentence)
{
    // $GPRMC,120004,V,,,,,,,270124,,,N*56
    EXPECT_EQ(ChecksumValidator::compute(
        "$GPRMC,120004,V,,,,,,,270124,,,N*56"),
        0x56u);
}

TEST(ChecksumValidatorTest, ComputeIgnoresDollarAndAsterisk)
{
    // compute должен учитывать только байты между $ и *
    // "AB" → 0x41 XOR 0x42 = 0x03
    EXPECT_EQ(ChecksumValidator::compute("$AB*03"), 0x03u);
}

TEST(ChecksumValidatorTest, ComputeSingleByte)
{
    // "$A*41" → только байт 'A' = 0x41
    EXPECT_EQ(ChecksumValidator::compute("$A*41"), 0x41u);
}

// ---------------------------------------------------------------------------
// validate() — проверяет, совпадает ли вычисленный checksum с указанным в '*XX'
// ---------------------------------------------------------------------------

TEST(ChecksumValidatorTest, ValidateCorrectChecksum)
{
    EXPECT_TRUE(ChecksumValidator::validate(
        "$GPRMC,120000,A,5545.1234,N,03739.5678,E,025.0,045.0,270124,,,A*70"));
}

TEST(ChecksumValidatorTest, ValidateCorrectGgaChecksum)
{
    EXPECT_TRUE(ChecksumValidator::validate(
        "$GPGGA,120000,5545.1234,N,03739.5678,E,1,10,0.8,150.0,M,14.0,M,,*4E"));
}

TEST(ChecksumValidatorTest, ValidateWrongChecksum)
{
    // Намеренно неверный checksum (*00 вместо *70)
    EXPECT_FALSE(ChecksumValidator::validate(
        "$GPRMC,120005,A,5545.1234,N,03739.5678,E,025.0,045.0,270124,,,A*00"));
}

TEST(ChecksumValidatorTest, ValidateCaseInsensitiveHex)
{
    // Checksum в нижнем регистре должен проходить наравне с верхним
    EXPECT_TRUE(ChecksumValidator::validate("$AB*03"));
    EXPECT_TRUE(ChecksumValidator::validate("$AB*03"));
}

// ---------------------------------------------------------------------------
// Edge cases
// ---------------------------------------------------------------------------

TEST(ChecksumValidatorTest, ValidateEmptyStringReturnsFalse)
{
    EXPECT_FALSE(ChecksumValidator::validate(""));
}

TEST(ChecksumValidatorTest, ValidateNoAsteriskReturnsFalse)
{
    EXPECT_FALSE(ChecksumValidator::validate("$GPRMC,120000,A"));
}

TEST(ChecksumValidatorTest, ValidateNoDollarReturnsFalse)
{
    EXPECT_FALSE(ChecksumValidator::validate("GPRMC,120000,A*70"));
}

TEST(ChecksumValidatorTest, ValidateTooShortAfterAsteriskReturnsFalse)
{
    // '*' есть, но за ним меньше двух символов
    EXPECT_FALSE(ChecksumValidator::validate("$AB*0"));
    EXPECT_FALSE(ChecksumValidator::validate("$AB*"));
}
