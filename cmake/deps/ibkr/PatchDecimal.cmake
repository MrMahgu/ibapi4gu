include_guard(GLOBAL)

function(ibapi4gu_create_patched_decimal input_file output_file expected_sha256)
  if(NOT EXISTS "${input_file}")
    message(FATAL_ERROR "IBKR Decimal.cpp was not found: ${input_file}")
  endif()

  file(SHA256 "${input_file}" _actual_sha256)
  if(NOT "${_actual_sha256}" STREQUAL "${expected_sha256}")
    message(FATAL_ERROR
      "Refusing to patch an unreviewed IBKR Decimal.cpp. Expected SHA-256 "
      "${expected_sha256}, got ${_actual_sha256}. Review the new IBKR source "
      "and update or remove the version-scoped patch."
    )
  endif()

  file(READ "${input_file}" _source)
  string(REGEX MATCHALL "unsigned int flags" _flag_declarations "${_source}")
  list(LENGTH _flag_declarations _flag_count)
  if(NOT _flag_count EQUAL 8)
    message(FATAL_ERROR
      "Expected eight uninitialized Decimal.cpp status words, found "
      "${_flag_count}. Refusing an ambiguous patch."
    )
  endif()
  string(REPLACE "unsigned int flags;" "unsigned int flags = 0;" _source "${_source}")

  set(_display_signature
    "std::string DecimalFunctions::decimalStringToDisplay(Decimal value) {"
  )
  string(FIND "${_source}" "${_display_signature}" _display_offset)
  if(_display_offset EQUAL -1)
    message(FATAL_ERROR "Could not locate DecimalFunctions::decimalStringToDisplay.")
  endif()
  string(SUBSTRING "${_source}" 0 ${_display_offset} _prefix)

  set(_replacement [=[std::string DecimalFunctions::decimalStringToDisplay(Decimal value) {
    const std::string text = decimalToString(value);

    if (text == "+NaN" || text == "-NaN" ||
        text == "+SNaN" || text == "-SNaN") {
        return "";
    }

    const std::string::size_type exponentPosition = text.find('E');
    if (exponentPosition == std::string::npos) {
        return text;
    }

    const bool negative = !text.empty() && text.front() == '-';
    const bool hasSign = !text.empty() &&
        (text.front() == '+' || text.front() == '-');
    const std::string::size_type coefficientBegin = hasSign ? 1 : 0;
    const std::string digits = text.substr(
        coefficientBegin, exponentPosition - coefficientBegin);
    if (digits.empty()) {
        return text;
    }
    for (const char digit : digits) {
        if (digit < '0' || digit > '9') {
            return text;
        }
    }

    std::string::size_type exponentIndex = exponentPosition + 1;
    int exponentSign = 1;
    if (exponentIndex < text.size() &&
        (text[exponentIndex] == '+' || text[exponentIndex] == '-')) {
        exponentSign = text[exponentIndex] == '-' ? -1 : 1;
        ++exponentIndex;
    }
    if (exponentIndex == text.size()) {
        return text;
    }

    int exponent = 0;
    for (; exponentIndex < text.size(); ++exponentIndex) {
        const char digit = text[exponentIndex];
        if (digit < '0' || digit > '9') {
            return text;
        }
        exponent = exponent * 10 + (digit - '0');
    }
    exponent *= exponentSign;

    const long long decimalPosition =
        static_cast<long long>(digits.size()) + exponent;
    std::string result = negative ? "-" : "";

    if (decimalPosition <= 0) {
        result += "0.";
        result.append(static_cast<std::string::size_type>(-decimalPosition), '0');
        result += digits;
    } else if (decimalPosition >= static_cast<long long>(digits.size())) {
        result += digits;
        result.append(
            static_cast<std::string::size_type>(
                decimalPosition - static_cast<long long>(digits.size())),
            '0');
    } else {
        const auto split = static_cast<std::string::size_type>(decimalPosition);
        result += digits.substr(0, split);
        result += '.';
        result += digits.substr(split);
    }

    return result;
}
]=])

  set(_patched_source "${_prefix}${_replacement}")
  get_filename_component(_output_directory "${output_file}" DIRECTORY)
  file(MAKE_DIRECTORY "${_output_directory}")

  set(_write_output TRUE)
  if(EXISTS "${output_file}")
    file(READ "${output_file}" _existing_output)
    if("${_existing_output}" STREQUAL "${_patched_source}")
      set(_write_output FALSE)
    endif()
  endif()
  if(_write_output)
    file(WRITE "${output_file}" "${_patched_source}")
  endif()
endfunction()
