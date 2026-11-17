#ifndef PATH_SMOOTHER_CORE__PATH_MATH_HPP_
#define PATH_SMOOTHER_CORE__PATH_MATH_HPP_

#include <cstddef>
#include <vector>

namespace path_smoother_core
{

struct Point2D
{
  double x{0.0};
  double y{0.0};
};

using Path2D = std::vector<Point2D>;

// 折线总长度
double path_length(const Path2D & path);

// 相邻两段之间的最大转角（弧度），用来衡量"锯齿"程度
double max_turn_angle(const Path2D & path);

// 滑动平均：window 必须是正奇数；起点和终点保持不变
Path2D moving_average(const Path2D & path, std::size_t window);

}  // namespace path_smoother_core

#endif  // PATH_SMOOTHER_CORE__PATH_MATH_HPP_
