#include "path_smoother_core/path_math.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace path_smoother_core
{

double path_length(const Path2D & path)
{
  double len = 0.0;
  for (std::size_t i = 1; i < path.size(); ++i) {
    len += std::hypot(path[i].x - path[i - 1].x, path[i].y - path[i - 1].y);
  }
  return len;
}

double max_turn_angle(const Path2D & path)
{
  double worst = 0.0;
  for (std::size_t i = 1; i + 1 < path.size(); ++i) {
    const double a1 = std::atan2(path[i].y - path[i - 1].y, path[i].x - path[i - 1].x);
    const double a2 = std::atan2(path[i + 1].y - path[i].y, path[i + 1].x - path[i].x);
    double d = std::fabs(a2 - a1);
    if (d > M_PI) {
      d = 2.0 * M_PI - d;
    }
    worst = std::max(worst, d);
  }
  return worst;
}

Path2D moving_average(const Path2D & path, std::size_t window)
{
  if (window == 0 || window % 2 == 0) {
    throw std::invalid_argument("window must be a positive odd number");
  }
  if (path.size() < 3 || window == 1) {
    return path;
  }
  const std::size_t half = window / 2;
  Path2D out(path.size());
  for (std::size_t i = 0; i < path.size(); ++i) {
    // 端点附近窗口缩小成对称的，保证起点终点不动
    const std::size_t h = std::min({half, i, path.size() - 1 - i});
    double sx = 0.0, sy = 0.0;
    for (std::size_t j = i - h; j <= i + h; ++j) {
      sx += path[j].x;
      sy += path[j].y;
    }
    const double n = static_cast<double>(2 * h + 1);
    out[i] = {sx / n, sy / n};
  }
#ifdef PATH_SMOOTHER_INJECT_BUG
  out.back() = out[out.size() - 2];  // 故意的 bug：终点被改掉
#endif
  return out;
}

}  // namespace path_smoother_core
