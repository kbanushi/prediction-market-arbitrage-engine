#pragma once

#include <vector>
#include <unordered_map>
#include "types.hpp"

std::vector<Opportunity> evaluate(const std::unordered_map<uint32_t, Market>& markets, const std::vector<Constraint>& constraints);