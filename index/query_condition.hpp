#pragma once

#include <string>

enum class ConditionType {
  Equality,
  Range,
};

/**
 * Supports:
 *  - Equality: field = value
 *  - Range: field in [start, end]
 * 
 * Example:
 *  QueryCondition cond = QueryCondition::equality("purpose", "marketing")
 * 
 */
struct QueryCondition {
  ConditionType type;
  std::string field;
  
  // for Equality
  std::string eq_value;

  // for Range
   std::string start, end;

  static auto equality(std::string f, std::string v) -> QueryCondition {
    return {ConditionType::Equality, std::move(f), std::move(v), {}, {}};
  }

  // TODO
  static auto range(std::string f, std::string lo, std::string hi) -> QueryCondition {
    return {ConditionType::Range, std::move(f), {}, std::move(lo), std::move(hi)};
  }

};