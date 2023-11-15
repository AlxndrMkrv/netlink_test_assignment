#pragma once

#include <linux/rtnetlink.h>
#include <ctime>
#include <cinttypes>

namespace Message {

struct LinkRequest
{
    nlmsghdr hdr;
    ifinfomsg ifi;
    uint8_t buffer [1024];

    LinkRequest(const uint16_t type, const uint16_t flags) :
        hdr{.nlmsg_len = NLMSG_LENGTH(sizeof(ifinfomsg)),
                      .nlmsg_type = type,
                      .nlmsg_flags = flags,
                      .nlmsg_seq = static_cast<uint32_t>(std::time(0))},
        ifi{.ifi_family = AF_UNSPEC} {}
};

}
