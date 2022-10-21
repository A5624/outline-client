/**
 * @file network_monitor.cpp
 * @author The Outline Authors
 * @brief
 * This file contains the implementation code of network_monitor.h.
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

#include <array>
#include <cstddef>      // size_t, byte
#include <cstring>      // memset
#include <system_error>
#include <type_traits>  // underlying_type_t, is_integral

#include <linux/netlink.h>
#include <linux/rtnetlink.h>

#include <boost/asio.hpp>

#include "logger.h"
#include "network_monitor.h"

using namespace outline;

static const std::size_t kRecvBufferSize = 4096;

namespace outline {

static_assert(std::is_integral<std::underlying_type_t<NetworkChangeEvent>>::value);

NetworkChangeEvent operator|(NetworkChangeEvent lhs, NetworkChangeEvent rhs) noexcept {
  using IntType = std::underlying_type_t<NetworkChangeEvent>;
  return static_cast<NetworkChangeEvent>(static_cast<IntType>(lhs) | static_cast<IntType>(rhs));
}

NetworkChangeEvent& operator|=(NetworkChangeEvent& lhs, NetworkChangeEvent rhs) noexcept {
  return lhs = lhs | rhs;
}

NetworkChangeEvent operator&(NetworkChangeEvent lhs, NetworkChangeEvent rhs) noexcept {
  using IntType = std::underlying_type_t<NetworkChangeEvent>;
  return static_cast<NetworkChangeEvent>(static_cast<IntType>(lhs) & static_cast<IntType>(rhs));
}

std::string to_string(NetworkChangeEvent evt) {
  if (evt == NetworkChangeEvent::kNone) {
    return "None";
  }
  std::string result{};
  if ((evt & NetworkChangeEvent::kNicChanged) == NetworkChangeEvent::kNicChanged) {
    result += "NIC";
  }
  if ((evt & NetworkChangeEvent::kAddressChanged) == NetworkChangeEvent::kAddressChanged) {
    result += (result.empty() ? "IP" : " IP");
  }
  if ((evt & NetworkChangeEvent::kRouteChanged) == NetworkChangeEvent::kRouteChanged) {
    result += (result.empty() ? "Route" : " Route");
  }
  return result;
}

}

NetworkMonitor::NetworkMonitor(const boost::asio::any_io_executor &io_context)
  : netlink_socket_{io_context}
{
  using namespace boost::asio::generic;

  sockaddr_nl sa;
  std::memset(&sa, 0, sizeof(sa));
  sa.nl_family = AF_NETLINK;
  sa.nl_groups = RTMGRP_LINK         // network card change
               | RTMGRP_IPV4_IFADDR  // IPv4 address change
               | RTMGRP_IPV4_ROUTE   // IPv4 route table change
               | RTMGRP_IPV6_IFADDR  // IPv6 address change
               | RTMGRP_IPV6_ROUTE;  // IPv6 route table change

  netlink_socket_.open({AF_NETLINK, NETLINK_ROUTE});
  netlink_socket_.bind({&sa, sizeof(sa)});

  logger.info("network monitor initialized");
}

NetworkMonitor::~NetworkMonitor() {
  logger.info("network monitor destroyed");
}

boost::asio::awaitable<NetworkChangeEvent> NetworkMonitor::WaitForChangeEvent() {
  using namespace boost::asio;

  std::array<std::byte, kRecvBufferSize> buf;
  auto received_events = NetworkChangeEvent::kNone;

  do {
    auto len = co_await netlink_socket_.async_receive(boost::asio::buffer(buf), use_awaitable);

    // https://linux.die.net/man/7/netlink
    for (auto msg = reinterpret_cast<nlmsghdr*>(buf.data()); NLMSG_OK(msg, len); msg = NLMSG_NEXT(msg, len)) {
      switch (msg->nlmsg_type) {
        case NLMSG_DONE:
          goto EndOfMultiPartMsg;
        case NLMSG_ERROR:
        {
          auto err = reinterpret_cast<nlmsgerr*>(NLMSG_DATA(msg));
          throw std::system_error{err->error, std::system_category()};
        }
        case RTM_NEWLINK:
          received_events |= NetworkChangeEvent::kNicChanged;
          break;
        case RTM_NEWADDR:
        case RTM_DELADDR:
          received_events |= NetworkChangeEvent::kAddressChanged;
          break;
        case RTM_NEWROUTE:
        case RTM_DELROUTE:
          received_events |= NetworkChangeEvent::kRouteChanged;
          break;
      }
    }

EndOfMultiPartMsg:
    ;
  } while (received_events == NetworkChangeEvent::kNone);

  co_return received_events;
}
