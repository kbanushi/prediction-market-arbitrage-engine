#include "polymarket_adapter.hpp"

#include <limits>

namespace polymarket {
    bool parse_scaled_decimal(std::string_view value, uint32_t scale, uint32_t& out){
        if (value.empty()){
            return false;
        }

        uint64_t interger_part = 0, fractional_part = 0;
        uint32_t fractional_scale = scale;
        bool seen_decimal = false, seen_digit = false;

        for (char ch : value){
            if (ch == '.'){
                if (seen_decimal){
                    return false;
                }

                seen_decimal = true;
                continue;
            }

            if (ch < '0' || ch > '9'){
                return false;
            }

            seen_digit = true;
            const uint32_t digit = static_cast<uint32_t>(ch - '0');

            if (!seen_decimal){
                interger_part = interger_part * 10 + digit;
                
                if (interger_part > std::numeric_limits<uint32_t>::max()){
                    return false;
                }
            }
            else{
                if (fractional_scale > 1){
                    fractional_scale /= 10;
                    fractional_part += static_cast<uint64_t>(digit) * fractional_scale;
                }
                else{
                    // More precision than our scale supports
                    if (digit != 0){
                        return false;
                    }
                }
            }
        }

        if (!seen_digit){
            return false;
        }

        const uint64_t scaled = interger_part * scale + fractional_part;
        if (scaled > std::numeric_limits<uint64_t>::max()){
            return false;
        }

        out = static_cast<uint32_t>(scaled);
        return true;
    }
} // namespace polymarket