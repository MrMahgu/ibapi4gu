#include <cmath>
#include <iostream>
#include <string>
#include <string_view>

#include <client/Decimal.h>

namespace {

int failures = 0;

void expect_display(std::string_view input, std::string_view expected) {
    const Decimal value = DecimalFunctions::stringToDecimal(std::string(input));
    const std::string actual = DecimalFunctions::decimalStringToDisplay(value);
    if (actual != expected) {
        std::cerr << "display(" << input << "): expected '" << expected
                  << "', got '" << actual << "'\n";
        ++failures;
    }
}

void expect_near(double actual, double expected, std::string_view operation) {
    if (std::abs(actual - expected) > 1e-12) {
        std::cerr << operation << ": expected " << expected << ", got "
                  << actual << '\n';
        ++failures;
    }
}

} // namespace

int main() {
    expect_display("1E+2", "100");
    expect_display("-1E+3", "-1000");
    expect_display("1E-2", "0.01");
    expect_display("123.45", "123.45");
    expect_display("100", "100");
    expect_display("42E+0", "42");
    expect_display("2147483647", "");

    const Decimal one_and_a_quarter =
        DecimalFunctions::stringToDecimal("1.25");
    const Decimal two = DecimalFunctions::stringToDecimal("2");
    expect_near(
        DecimalFunctions::decimalToDouble(
            DecimalFunctions::add(one_and_a_quarter, two)),
        3.25,
        "add");
    expect_near(
        DecimalFunctions::decimalToDouble(
            DecimalFunctions::sub(one_and_a_quarter, two)),
        -0.75,
        "sub");
    expect_near(
        DecimalFunctions::decimalToDouble(
            DecimalFunctions::mul(one_and_a_quarter, two)),
        2.5,
        "mul");
    expect_near(
        DecimalFunctions::decimalToDouble(
            DecimalFunctions::div(one_and_a_quarter, two)),
        0.625,
        "div");
    expect_near(
        DecimalFunctions::decimalToDouble(
            DecimalFunctions::doubleToDecimal(12.5)),
        12.5,
        "double round trip");

    return failures == 0 ? 0 : 1;
}
