/**
 * @file network_monitor.h
 * @author The Outline Authors
 * @brief
 * This file contains interface definitions of a Linux ip table monitor.
 * Callers can use it to receive routing and link updates change events.
 *
 * @copyright Copyright (c) 2022 The Outline Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <string>

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/generic/raw_protocol.hpp>

namespace outline {

/**
 * @brief The enumeration flags representing the kind of network change event.
 *        This is a bit-flag enumeration, please use `& ==` instead of `==` to
 *        check the specific value.
 */
enum class NetworkChangeEvent : unsigned int {
  /** @brief Nothing has changed. */
  kNone           = 0b00000000,
  /** @brief The network interface car has changed. */
  kNicChanged     = 0b00000001,
  /** @brief The IPv4 or IPv6 address has changed. */
  kAddressChanged = 0b00000010,
  /** @brief The routing table (v4 or v6) has changed. */
  kRouteChanged   = 0b00000100,
};

/**
 * @brief Strongly typed `|` operator for NetworkChangeEvent.
 */
NetworkChangeEvent operator|(NetworkChangeEvent, NetworkChangeEvent) noexcept;

/**
 * @brief Strongly typed `|=` operator for NetworkChangeEvent.
 */
NetworkChangeEvent& operator|=(NetworkChangeEvent&, NetworkChangeEvent) noexcept;

/**
 * @brief Strongly typed `&` operator for NetworkChangeEvent.
 */
NetworkChangeEvent operator&(NetworkChangeEvent, NetworkChangeEvent) noexcept;

/**
 * @brief Get the string representation of NetworkChangeEvent bit flags.
 */
std::string to_string(NetworkChangeEvent);

/**
 * @brief Provide methods to receive various network related change
 *        notifications asynchronously using Linux netlink API.
 */
class NetworkMonitor final {
public:
  /**
   * @brief Construct a new NetworkMonitor object. Please note that this
   *        constructor may throw errors if the OS does not support the
   *        required netlink socket protocol.
   *
   * @param io_context Any Boost ASIO executor (like io_context) which will be
   *                   passed to the underlying socket.
   */
  NetworkMonitor(const boost::asio::any_io_executor &io_context);
  ~NetworkMonitor();

private:
  // Non-copyable or moveable
  NetworkMonitor(const NetworkMonitor &) = delete;
  NetworkMonitor& operator=(const NetworkMonitor &) = delete;
  NetworkMonitor(NetworkMonitor &&) noexcept = delete;
  NetworkMonitor& operator=(NetworkMonitor &&) noexcept = delete;

public:
  /**
   * @brief Asynchronously wait for the next network change event.
   * @remarks thread unsafe, at most one outstanding call.
   *
   * @return boost::asio::awaitable<NetworkChangeEvent> One or more network
   *         change event flag(s).
   */
  boost::asio::awaitable<NetworkChangeEvent> WaitForChangeEvent();

private:
  boost::asio::generic::raw_protocol::socket netlink_socket_;
};

}
