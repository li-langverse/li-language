#pragma once

#include "li/mir.hpp"

namespace li {

/** Algebraic / compensated rewrites when {@link MirModule::fp_numerically_stable}. */
void apply_numerical_stability(MirModule& mir);

}  // namespace li
