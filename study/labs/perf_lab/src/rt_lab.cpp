// 练习「实时编程」：1 kHz 周期循环的唤醒延迟、缺页和堆分配
// 运行示例（全部参数见 labs/realtime-jitter.md）：
//   ros2 run perf_lab rt_lab --loop sleep
//   ros2 run perf_lab rt_lab --loop sleep --alloc 4000000
//   ros2 run perf_lab rt_lab --loop sleep --alloc 4000000 --lock
//   ros2 run perf_lab rt_lab --loop timer
//   ros2 run perf_lab rt_lab --count-new
#include <pthread.h>
#include <sched.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <time.h>

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>
#include <thread>
#include <vector>

#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/int32.hpp"
#include "std_msgs/msg/string.hpp"

// ---- 统计全局 operator new 的调用次数 -------------------------------------
static std::atomic<bool> g_counting{false};
static std::atomic<long> g_news{0};

void * operator new(std::size_t size)
{
  if (g_counting.load(std::memory_order_relaxed)) {g_news.fetch_add(1);}
  if (void * p = std::malloc(size ? size : 1)) {return p;}
  throw std::bad_alloc();
}
void operator delete(void * p) noexcept {std::free(p);}
void operator delete(void * p, std::size_t) noexcept {std::free(p);}

// ---- 工具 -----------------------------------------------------------------
static long minor_faults()
{
  rusage u{};
  getrusage(RUSAGE_SELF, &u);
  return u.ru_minflt;
}

static int64_t to_ns(const timespec & t) {return int64_t(t.tv_sec) * 1000000000LL + t.tv_nsec;}

static void report(const char * title, std::vector<int64_t> lat, long faults)
{
  std::sort(lat.begin(), lat.end());
  double sum = 0;
  for (auto v : lat) {sum += v;}
  auto pct = [&](double p) {return lat[std::min(lat.size() - 1, size_t(p * lat.size()))] / 1000.0;};
  std::printf("%s: n=%zu  mean %.1f us  p50 %.1f  p99 %.1f  max %.1f us  minor faults %ld\n",
    title, lat.size(), sum / lat.size() / 1000.0, pct(0.5), pct(0.99), lat.back() / 1000.0, faults);
}

// 每个周期要做的"控制计算"：可选地分配并写满一块内存（模拟在控制环里用 std::vector 扩容）
static void work(size_t alloc_bytes)
{
  if (alloc_bytes == 0) {return;}
  std::vector<char> buf(alloc_bytes);
  for (size_t i = 0; i < buf.size(); i += 4096) {buf[i] = 1;}   // 每页写一次，触发缺页
  asm volatile ("" : : "r" (buf.data()) : "memory");
}

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  const auto args = rclcpp::remove_ros_arguments(argc, argv);
  std::string loop = "sleep";
  int period_us = 1000, iters = 3000, fifo = 0, load = 0;
  size_t alloc = 0;
  bool lock = false, count_new = false;
  for (size_t i = 1; i < args.size(); ++i) {
    const auto & a = args[i];
    auto next = [&]() {return i + 1 < args.size() ? args[++i] : std::string("0");};
    if (a == "--loop") {loop = next();}
    else if (a == "--period-us") {period_us = std::stoi(next());}
    else if (a == "--iters") {iters = std::stoi(next());}
    else if (a == "--alloc") {alloc = std::stoul(next());}
    else if (a == "--lock") {lock = true;}
    else if (a == "--fifo") {fifo = std::stoi(next());}
    else if (a == "--load") {load = std::stoi(next());}
    else if (a == "--count-new") {count_new = true;}
  }

  if (count_new) {
    // 每种写法发布 100 次，数 operator new 被调用了多少次（没有订阅者，跨进程路径）
    auto node = std::make_shared<rclcpp::Node>("rt_lab_alloc");
    auto ipub = node->create_publisher<std_msgs::msg::Int32>("i", 10);
    auto spub = node->create_publisher<std_msgs::msg::String>("s", 10);
    std_msgs::msg::Int32 pre_i;
    std_msgs::msg::String pre_s;
    pre_s.data = std::string(64, 'x');
    auto measure = [](const char * what, auto && fn) {
        g_news = 0;
        g_counting = true;
        for (int k = 0; k < 100; ++k) {fn(k);}
        g_counting = false;
        std::printf("%-44s %6.2f new/publish\n", what, g_news.load() / 100.0);
      };
    measure("Int32: make_unique + publish(move)", [&](int k) {
        auto m = std::make_unique<std_msgs::msg::Int32>(); m->data = k; ipub->publish(std::move(m));});
    measure("Int32: preallocated, publish(const &)", [&](int k) {
        pre_i.data = k; ipub->publish(pre_i);});
    measure("String(64 chars): make_unique + publish(move)", [&](int) {
        auto m = std::make_unique<std_msgs::msg::String>(); m->data = std::string(64, 'x');
        spub->publish(std::move(m));});
    measure("String(64 chars): preallocated, publish(const &)", [&](int) {spub->publish(pre_s);});
    rclcpp::shutdown();
    return 0;
  }

  if (lock) {
    // 文档：把已有和将来的内存都锁在 RAM 里，避免换页；再预先摸一遍栈
    if (mlockall(MCL_CURRENT | MCL_FUTURE) != 0) {
      std::printf("mlockall failed: %s\n", std::strerror(errno));
    } else {
      std::printf("mlockall ok\n");
    }
    volatile char stack[256 * 1024];
    for (size_t i = 0; i < sizeof(stack); i += 4096) {stack[i] = 0;}
    (void)stack;
  }
  if (fifo > 0) {
    sched_param sp{};
    sp.sched_priority = fifo;
    int rc = pthread_setschedparam(pthread_self(), SCHED_FIFO, &sp);
    std::printf("SCHED_FIFO %d: %s\n", fifo, rc == 0 ? "ok" : std::strerror(rc));
  }

  // 可选：开 N 个忙等线程制造 CPU 竞争
  std::atomic<bool> stop{false};
  std::vector<std::thread> burners;
  for (int k = 0; k < load; ++k) {
    burners.emplace_back([&stop]() {while (!stop) {asm volatile ("");}});
  }

  std::vector<int64_t> lat;
  lat.reserve(iters);
  const int64_t period_ns = int64_t(period_us) * 1000;

  if (loop == "sleep") {
    // 绝对时间睡眠：下一次唤醒时刻 = 起点 + k * 周期，误差不会累积
    timespec next{};
    clock_gettime(CLOCK_MONOTONIC, &next);
    const long f0 = minor_faults();
    for (int k = 0; k < iters; ++k) {
      int64_t target = to_ns(next) + period_ns;
      next.tv_sec = target / 1000000000LL;
      next.tv_nsec = target % 1000000000LL;
      clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, nullptr);
      timespec now{};
      clock_gettime(CLOCK_MONOTONIC, &now);
      lat.push_back(to_ns(now) - target);
      work(alloc);
    }
    report("sleep loop", lat, minor_faults() - f0);
  } else {
    // rclcpp 定时器 + 单线程执行器。定时器晚到超过一个周期时会跳过错过的周期，
    // 所以这里统计"相邻两次回调的间隔 - 周期"，并比较回调次数和应有次数
    auto node = std::make_shared<rclcpp::Node>("rt_lab");
    rclcpp::executors::SingleThreadedExecutor exec;
    exec.add_node(node);
    int k = 0;
    int64_t start = 0, prev = 0, end = 0;
    long f0 = 0;
    auto timer = node->create_wall_timer(std::chrono::microseconds(period_us), [&]() {
        timespec ts{};
        clock_gettime(CLOCK_MONOTONIC, &ts);
        const int64_t now = to_ns(ts);
        if (k == 0) {start = now; f0 = minor_faults();}
        else {lat.push_back(now - prev - period_ns);}
        prev = now;
        work(alloc);
        if (++k > iters) {end = now; exec.cancel();}
      });
    exec.spin();
    report("rclcpp timer (interval - period)", lat, minor_faults() - f0);
    std::printf("callbacks %d in %.1f ms, expected %.0f\n",
      iters, (end - start) / 1e6, double(end - start) / period_ns);
  }

  stop = true;
  for (auto & t : burners) {t.join();}
  rclcpp::shutdown();
  return 0;
}
