// 练习：Deadline / Liveliness / Lifespan 的设计语义到底是什么？
//
// 设计文档：ROS QoS - Deadline, Liveliness, and Lifespan
//   --case deadline-rmw  订阅回调很慢（每条 400 ms），上游 10 Hz 准时发：Deadline 会报错吗？
//   --case coalesce      上游 1 s 后停发，订阅端执行器被一个 1 s 的回调卡住：事件回调触发几次？
//   --case liveliness    MANUAL_BY_TOPIC（1 Hz 发布，租约 300 ms） vs AUTOMATIC（几乎不发布）
//                        加 --same-participant：看门狗和发布者在同一个 DDS 参与者里
//   --case lifespan      TransientLocal + Lifespan 500 ms，订阅者 0.2 s / 1.0 s 后加入各拿到几条？
//
// 每个节点用自己的执行器线程，避免"发布端被订阅端的慢回调拖住"干扰结论。
// 用法：ros2 run design_lab qos_semantics_lab --case <名字>
// 适用：ROS 2 Rolling（rclcpp 33.x，rmw_fastrtps_cpp）
#include <atomic>
#include <chrono>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/int64.hpp"

using namespace std::chrono_literals;
using Int64 = std_msgs::msg::Int64;
using Clock = std::chrono::steady_clock;

namespace
{
const Clock::time_point kStart = Clock::now();
double now_s() {return std::chrono::duration<double>(Clock::now() - kStart).count();}
int64_t now_ns()
{
  return std::chrono::duration_cast<std::chrono::nanoseconds>(
    Clock::now().time_since_epoch()).count();
}

// 每个节点一个执行器线程
class Runner
{
public:
  void add(const rclcpp::Node::SharedPtr & n)
  {
    rclcpp::ExecutorOptions eo;
    eo.context = n->get_node_base_interface()->get_context();  // 节点可能在另一个 context 里
    auto exec = std::make_shared<rclcpp::executors::SingleThreadedExecutor>(eo);
    exec->add_node(n);
    execs_.push_back(exec);
    threads_.emplace_back([exec]() {exec->spin();});
  }
  ~Runner()
  {
    for (auto & e : execs_) {e->cancel();}
    for (auto & t : threads_) {t.join();}
  }

private:
  std::vector<std::shared_ptr<rclcpp::executors::SingleThreadedExecutor>> execs_;
  std::vector<std::thread> threads_;
};

void case_deadline_rmw()
{
  auto pub_node = std::make_shared<rclcpp::Node>("fast_source");
  auto sub_node = std::make_shared<rclcpp::Node>("slow_consumer");
  auto qos = rclcpp::QoS(10).deadline(200ms);

  std::atomic<int> pub_missed{0}, sub_missed{0}, received{0};
  std::atomic<int64_t> max_age_ms{0};

  rclcpp::PublisherOptions po;
  po.event_callbacks.deadline_callback = [&](rclcpp::QOSDeadlineOfferedInfo &) {++pub_missed;};
  auto pub = pub_node->create_publisher<Int64>("cmd", qos, po);
  auto timer = pub_node->create_wall_timer(100ms, [&]() {
        Int64 m;
        m.data = now_ns();
        pub->publish(m);
      });

  rclcpp::SubscriptionOptions so;
  so.event_callbacks.deadline_callback = [&](rclcpp::QOSDeadlineRequestedInfo &) {++sub_missed;};
  auto sub = sub_node->create_subscription<Int64>("cmd", qos, [&](Int64::ConstSharedPtr m) {
        int64_t age = (now_ns() - m->data) / 1000000;
        if (age > max_age_ms) {max_age_ms = age;}
        ++received;
        std::this_thread::sleep_for(400ms);  // 模拟很慢的控制计算
      }, so);

  {
    Runner r;
    r.add(pub_node);
    r.add(sub_node);
    std::this_thread::sleep_for(4s);
  }
  std::printf("[deadline-rmw] 4 s 内：回调处理 %d 条，消息到回调时最老 %ld ms；"
    "订阅端 deadline missed %d 次，发布端 %d 次\n",
    received.load(), static_cast<long>(max_age_ms.load()), sub_missed.load(), pub_missed.load());
}

void case_coalesce()
{
  auto pub_node = std::make_shared<rclcpp::Node>("source_that_stops");
  auto sub_node = std::make_shared<rclcpp::Node>("busy_consumer");
  auto qos = rclcpp::QoS(10).deadline(100ms);

  auto pub = pub_node->create_publisher<Int64>("odom", qos);
  auto pub_timer = pub_node->create_wall_timer(50ms, [&]() {
        if (now_s() < 1.0) {Int64 m; m.data = 0; pub->publish(m);}  // 1 s 后停发，但进程还在
      });

  std::mutex mtx;
  std::vector<std::string> events;
  rclcpp::SubscriptionOptions so;
  so.event_callbacks.deadline_callback = [&](rclcpp::QOSDeadlineRequestedInfo & info) {
      std::lock_guard<std::mutex> lk(mtx);
      char buf[128];
      std::snprintf(buf, sizeof(buf), "  t=%.2f s  回调一次: total_count=%d total_count_change=%d",
        now_s(), info.total_count, info.total_count_change);
      events.emplace_back(buf);
    };
  auto sub = sub_node->create_subscription<Int64>("odom", qos, [](Int64::ConstSharedPtr) {}, so);
  // 同一个执行器里的一个"重活"：t≈1.2 s 时阻塞 1 s
  bool done = false;
  auto busy = sub_node->create_wall_timer(1200ms, [&]() {
        if (!done) {done = true; std::this_thread::sleep_for(1s);}
      });

  {
    Runner r;
    r.add(pub_node);
    r.add(sub_node);
    std::this_thread::sleep_for(2600ms);
  }
  std::printf("[coalesce] 上游 t=1.0 s 停发，订阅端执行器在 t≈1.2~2.2 s 被卡住，deadline=100 ms\n");
  for (const auto & e : events) {std::printf("%s\n", e.c_str());}
}

void case_liveliness(bool same_participant)
{
  auto pub_node = std::make_shared<rclcpp::Node>("liveliness_pub");
  // 默认让"看门狗"节点用另一个 context，也就是另一个 DDS 参与者（相当于另一个进程）；
  // --same-participant 时和发布者共用一个参与者（相当于放进同一个组件容器）
  rclcpp::NodeOptions watch_opts;
  std::shared_ptr<rclcpp::Context> ctx2;
  if (!same_participant) {
    ctx2 = std::make_shared<rclcpp::Context>();
    rclcpp::InitOptions io;
    io.auto_initialize_logging(false);
    ctx2->init(0, nullptr, io);
    watch_opts.context(ctx2);
  }
  auto sub_node = std::make_shared<rclcpp::Node>("liveliness_watch", watch_opts);

  auto manual_qos = rclcpp::QoS(10).liveliness(rclcpp::LivelinessPolicy::ManualByTopic)
    .liveliness_lease_duration(300ms);
  auto auto_qos = rclcpp::QoS(10).liveliness(rclcpp::LivelinessPolicy::Automatic)
    .liveliness_lease_duration(300ms);

  auto manual_pub = pub_node->create_publisher<Int64>("planner_heartbeat", manual_qos);
  auto auto_pub = pub_node->create_publisher<Int64>("idle_topic", auto_qos);  // 只在 0.5 s 发一条
  auto timer = pub_node->create_wall_timer(1s, [&]() {Int64 m; m.data = 0; manual_pub->publish(m);});

  std::mutex mtx;
  std::vector<std::string> events;
  auto make_opts = [&](const std::string & tag) {
      rclcpp::SubscriptionOptions so;
      so.event_callbacks.liveliness_callback = [&, tag](rclcpp::QOSLivelinessChangedInfo & i) {
          std::lock_guard<std::mutex> lk(mtx);
          char buf[160];
          std::snprintf(buf, sizeof(buf), "  t=%.2f s  %-9s alive=%d not_alive=%d",
            now_s(), tag.c_str(), i.alive_count, i.not_alive_count);
          events.emplace_back(buf);
        };
      return so;
    };
  auto s1 = sub_node->create_subscription<Int64>("planner_heartbeat", manual_qos,
      [](Int64::ConstSharedPtr) {}, make_opts("MANUAL"));
  auto s2 = sub_node->create_subscription<Int64>("idle_topic", auto_qos,
      [](Int64::ConstSharedPtr) {}, make_opts("AUTOMATIC"));

  {
    Runner r;
    r.add(pub_node);
    r.add(sub_node);
    std::this_thread::sleep_for(500ms);
    Int64 m;
    m.data = 1;
    auto_pub->publish(m);  // t≈0.5 s：AUTOMATIC 只发这一条
    std::this_thread::sleep_for(2000ms);
    auto_pub.reset();  // t≈2.5 s：销毁 AUTOMATIC 的发布者（模拟节点退出）
    std::this_thread::sleep_for(1000ms);
  }
  if (ctx2) {ctx2->shutdown("done");}
  std::printf("[liveliness%s] 租约 300 ms；MANUAL_BY_TOPIC 每 1 s 发一条；"
    "AUTOMATIC 只在 0.5 s 发一条，2.5 s 被销毁\n", same_participant ? " 同一参与者" : "");
  for (const auto & e : events) {std::printf("%s\n", e.c_str());}
}

void case_lifespan()
{
  auto pub_node = std::make_shared<rclcpp::Node>("goal_board");
  auto qos = rclcpp::QoS(10).reliable().transient_local().lifespan(500ms);
  auto pub = pub_node->create_publisher<Int64>("latest_goals", qos);
  for (int i = 0; i < 5; ++i) {Int64 m; m.data = i; pub->publish(m);}

  auto late_join = [&](std::chrono::milliseconds delay) {
      std::this_thread::sleep_for(delay);
      auto n = std::make_shared<rclcpp::Node>("late_" + std::to_string(delay.count()));
      int got = 0;
      auto sub = n->create_subscription<Int64>("latest_goals", qos,
          [&](Int64::ConstSharedPtr) {++got;});
      rclcpp::executors::SingleThreadedExecutor ex;
      ex.add_node(n);
      auto t0 = Clock::now();
      while (Clock::now() - t0 < 500ms) {ex.spin_some(10ms);}
      return got;
    };
  rclcpp::executors::SingleThreadedExecutor e;
  e.add_node(pub_node);
  std::thread spin([&]() {e.spin();});
  int early = late_join(200ms);             // t≈0.2 s 加入
  int late = late_join(300ms);              // t≈1.0 s 加入（0.2+0.5 等待+0.3）
  e.cancel();
  spin.join();
  std::printf("[lifespan] 发布 5 条（TransientLocal，lifespan 500 ms）：t≈0.2 s 加入的订阅者收到 %d 条，"
    "t≈1.0 s 加入的收到 %d 条\n", early, late);
}
}  // namespace

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  std::string c = "deadline-rmw";
  bool same_participant = false;
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--case") == 0 && i + 1 < argc) {c = argv[i + 1];}
    if (std::strcmp(argv[i], "--same-participant") == 0) {same_participant = true;}
  }
  if (c == "deadline-rmw") {
    case_deadline_rmw();
  } else if (c == "coalesce") {
    case_coalesce();
  } else if (c == "liveliness") {
    case_liveliness(same_participant);
  } else if (c == "lifespan") {
    case_lifespan();
  } else {
    std::fprintf(stderr, "未知 --case %s\n", c.c_str());
  }
  rclcpp::shutdown();
  return 0;
}
