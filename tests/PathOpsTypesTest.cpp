/*
 * Copyright 2013 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "modules/pathops/src/SkPathOpsTypes.h"
#include "tests/Test.h"

#include <array>
#include <cstddef>

static const double roughlyTests[][2] = {
    {5.0402503619650929e-005, 4.3178054475078825e-005}
};

static const size_t roughlyTestsCount = std::size(roughlyTests);

DEF_TEST(PathOpsRoughly, reporter) {
    for (size_t index = 0; index < roughlyTestsCount; ++index) {
        bool equal = RoughlyEqualUlps(roughlyTests[index][0], roughlyTests[index][1]);
        REPORTER_ASSERT(reporter, equal);
    }
}
