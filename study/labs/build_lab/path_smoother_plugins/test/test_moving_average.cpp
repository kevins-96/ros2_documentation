#include <gtest/gtest.h>

#include <memory>
#include <stdexcept>

#include "../src/moving_average_smoother.hpp"
#include "rclcpp/rclcpp.hpp"

// 测试夹具：每个 TEST_F 前 init，后 shutdown，保证测试之间互不影响
class MovingAverageTest : public ::testing::Test
{
protected:
  void SetUp() override {rclcpp::init(0, nullptr);}
  void TearDown() override {rclcpp::shutdown();}
};

TEST_F(MovingAverageTest, ReadsWindowFromParameter)
{
  auto node = std::make_shared<rclcpp::Node>(
    "test_node", rclcpp::NodeOptions().append_parameter_override("s.window", 7));
  path_smoother_plugins::MovingAverageSmoother s;
  s.configure(node, "s");
  EXPECT_EQ(s.window(), 7u);
}

TEST_F(MovingAverageTest, RejectsEvenWindow)
{
  auto node = std::make_shared<rclcpp::Node>(
    "test_node", rclcpp::NodeOptions().append_parameter_override("s.window", 4));
  path_smoother_plugins::MovingAverageSmoother s;
  EXPECT_THROW(s.configure(node, "s"), std::invalid_argument);
}
