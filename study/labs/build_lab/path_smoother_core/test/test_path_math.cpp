#include <gtest/gtest.h>

#include <cmath>
#include <stdexcept>

#include "path_smoother_core/path_math.hpp"

using path_smoother_core::Path2D;

namespace
{
Path2D zigzag(int n)
{
  Path2D p;
  for (int i = 0; i < n; ++i) {
    p.push_back({static_cast<double>(i), (i % 2) ? 1.0 : 0.0});
  }
  return p;
}
}  // namespace

TEST(PathMath, LengthOfStraightLine)
{
  Path2D p{{0, 0}, {3, 4}, {6, 8}};
  EXPECT_DOUBLE_EQ(path_smoother_core::path_length(p), 10.0);
}

TEST(PathMath, MovingAverageKeepsEndpoints)
{
  const Path2D raw = zigzag(11);
  const Path2D out = path_smoother_core::moving_average(raw, 3);
  ASSERT_EQ(out.size(), raw.size());
  EXPECT_DOUBLE_EQ(out.front().x, raw.front().x);
  EXPECT_DOUBLE_EQ(out.front().y, raw.front().y);
  EXPECT_DOUBLE_EQ(out.back().x, raw.back().x);
  EXPECT_DOUBLE_EQ(out.back().y, raw.back().y);
}

TEST(PathMath, MovingAverageReducesTurning)
{
  const Path2D raw = zigzag(11);
  const Path2D out = path_smoother_core::moving_average(raw, 5);
  EXPECT_LT(path_smoother_core::max_turn_angle(out), path_smoother_core::max_turn_angle(raw));
  EXPECT_LT(path_smoother_core::path_length(out), path_smoother_core::path_length(raw));
}

TEST(PathMath, EvenWindowThrows)
{
  EXPECT_THROW(path_smoother_core::moving_average(zigzag(5), 4), std::invalid_argument);
}
