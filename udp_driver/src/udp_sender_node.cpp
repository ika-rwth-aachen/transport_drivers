// Copyright 2021 LeoDrive.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Developed by LeoDrive, 2021

#include "udp_driver/udp_sender_node.hpp"

#include <memory>
#include <string>

namespace lc = rclcpp_lifecycle;
using LNI = rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface;
using lifecycle_msgs::msg::State;

namespace drivers
{
namespace udp_driver
{

UdpSenderNode::UdpSenderNode(const rclcpp::NodeOptions & options)
: lc::LifecycleNode("udp_sender_node", options),
  m_ctx(new IoContext(1))
{
  get_params();
}

UdpSenderNode::UdpSenderNode(
  const rclcpp::NodeOptions & options,
  const std::shared_ptr<IoContext> & ctx)
: lc::LifecycleNode("udp_sender_node", options),
  m_ctx{ctx}
{
  get_params();
}

LNI::CallbackReturn UdpSenderNode::on_configure(const lc::State & state)
{
  (void)state;

  m_udp_driver = std::make_unique<UdpDriver>(*m_ctx);

  try {
    m_udp_driver->init_sender(m_ip, m_port);
    if (!m_udp_driver->sender()->isOpen()) {
      m_udp_driver->sender()->open();
    }
  } catch (const std::exception & ex) {
    RCLCPP_ERROR(
      get_logger(), "Error creating UDP sender: %s:%i - %s",
      m_ip.c_str(), m_port, ex.what());
    return LNI::CallbackReturn::FAILURE;
  }

  auto qos = rclcpp::QoS(rclcpp::KeepLast(32)).best_effort();
  auto callback = std::bind(&UdpSenderNode::subscriber_callback, this, std::placeholders::_1);

  m_subscriber = this->create_subscription<std_msgs::msg::Int32>(
    "udp_write", qos, callback);

  RCLCPP_DEBUG(get_logger(), "UDP sender successfully configured.");

  return LNI::CallbackReturn::SUCCESS;
}

LNI::CallbackReturn UdpSenderNode::on_activate(const lc::State & state)
{
  (void)state;
  RCLCPP_DEBUG(get_logger(), "UDP sender activated.");
  return LNI::CallbackReturn::SUCCESS;
}

LNI::CallbackReturn UdpSenderNode::on_deactivate(const lc::State & state)
{
  (void)state;
  RCLCPP_DEBUG(get_logger(), "UDP sender deactivated.");
  return LNI::CallbackReturn::SUCCESS;
}

LNI::CallbackReturn UdpSenderNode::on_cleanup(const lc::State & state)
{
  (void)state;
  m_udp_driver->sender()->close();
  m_udp_driver.reset();
  m_subscriber.reset();
  RCLCPP_DEBUG(get_logger(), "UDP sender cleaned up.");
  return LNI::CallbackReturn::SUCCESS;
}

LNI::CallbackReturn UdpSenderNode::on_shutdown(const lc::State & state)
{
  (void)state;
  RCLCPP_DEBUG(get_logger(), "UDP sender shutting down.");
  return LNI::CallbackReturn::SUCCESS;
}

void UdpSenderNode::get_params()
{
  m_ip = declare_parameter("ip").get<std::string>();
  m_port = declare_parameter("port").get<int16_t>();

  RCLCPP_INFO(get_logger(), "ip: %s", m_ip.c_str());
  RCLCPP_INFO(get_logger(), "port: %i", m_port);
}

void UdpSenderNode::subscriber_callback(std_msgs::msg::Int32::SharedPtr msg)
{
  if (this->get_current_state().id() == State::PRIMARY_STATE_ACTIVE) {
    MutSocketBuffer out;
    drivers::common::convertFromRos2Message(msg, out);

    m_udp_driver->sender()->asyncSend(out);
  }
}

}  // namespace udp_driver
}  // namespace drivers