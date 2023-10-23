# `json.hpp` Modification lists

## IMPORTANT

Since SGX cannot obtain locale information for security reasons, the Japanese locale settings are hard-coded. It is recommended to rewrite them according to your region.

## Global settings

**Modification**
- Add `#define JSON_NO_IO`

**Reason**
- `std::FILE` is not available on SGX.

## Class serializer
**Modifications**
- Comment out `#include <clocale>`.
- Comment out `const std::lconv* loc = nullptr;`.
- Comment out `loc(std::localeconv())`.
- Hard code `thousands_sep` to `'\0'`.
- Hard code `decimal_point` to `'.'`.

**Reason**  
- `localeconv()` is not available on SGX.

## class lexer : public lexer_base\<BasicJsonType>
**Modification**
- Modify `get_decimal_point()` to always return `'.'`.

**Reason**  
- `localeconv()` is not available on SGX.