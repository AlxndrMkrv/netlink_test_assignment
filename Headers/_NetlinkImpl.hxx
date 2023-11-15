#pragma once

#include <string>
#include <linux/netlink.h>
#include <functional>

#include "Application.hxx"
#include "Socket.hxx"
#include "Request.hxx"


// Returns a pointer to the free space AFTER the msg
constexpr rtattr * NLMSG_TAIL (nlmsghdr *msg) {
    return reinterpret_cast<rtattr *>(reinterpret_cast<size_t>(msg) +
                                      NLMSG_ALIGN(msg->nlmsg_len));
}

// Calculates size for the given attribute assuming that everything behind it
// are nested attributes.
constexpr size_t NLMSG_NESTED_RTA_SIZE (nlmsghdr *msg, rtattr *attr) {
    return reinterpret_cast<size_t>(NLMSG_TAIL(msg)) -
           reinterpret_cast<size_t>(attr);
}

// Auxiliary class that helps to create empty attributes
struct EmptyAttr {};

//
enum class ErrorCode { Success, DumpInconsistent };


/* The class taking on all dirty work of Netlink communication */
class _NetlinkImpl : public ApplicationData
{
    using ErrCallback = std::function<void(nlmsgerr *)>;
    using MsgCallback = std::function<void(nlmsghdr *)>;
    using AttrCallback = std::function<void(unsigned short,
                                            const rtattr * const)>;

protected:
    ErrorCode talkWithKernel(Message::LinkRequest &rq,
                             ErrCallback errHandle = nullptr,
                             MsgCallback msgHandle = nullptr);

    std::string_view getOperstate (const rtattr * const attr) const;
    std::string getBridgeId (const rtattr * const attr) const;

    void parseAttrs(const nlmsghdr *const msg, AttrCallback handle);
    void parseNestedAttrs(const rtattr *const parent, AttrCallback handle);


    template <class T>
    T readAttr (const rtattr *const attr) const
    {
        if constexpr (std::is_same_v<T, std::string_view>)
            return std::string_view(
                reinterpret_cast<const char *>(RTA_DATA(attr)));

        else if constexpr (std::is_same_v<T, unsigned char>)
            return *reinterpret_cast<unsigned char *>(RTA_DATA(attr));

        else if constexpr (std::is_same_v<T, unsigned short>)
            return *reinterpret_cast<unsigned short *>(RTA_DATA(attr));

        else if constexpr (std::is_same_v<T, unsigned int>)
            return *reinterpret_cast<unsigned int *>RTA_DATA(attr);

        else if constexpr (std::is_same_v<T, ifla_bridge_id*>)
            return reinterpret_cast<ifla_bridge_id *>(RTA_DATA(attr));

        else
            static_assert(false, "Don't know how to convert");
    }

    template <class T, class TMessage>
    rtattr * addAttr (nlmsghdr * msg, const uint16_t &type,
                      const T &value = T())
    {
        rtattr * attr = NLMSG_TAIL(msg);
        int size;

        auto add = [&](){
            // Check if attribute fit into the message
            if (NLMSG_ALIGN(msg->nlmsg_len) + RTA_ALIGN(size) > sizeof(TMessage))
                throw std::runtime_error(
                    std::format("Attribute {} won't fit into message", type));

            attr->rta_type = type;
            attr->rta_len = size;
        };

        if constexpr (std::is_same_v<T, std::string>) {
            size = RTA_LENGTH(value.size() + 1);
            add();
            std::memcpy(RTA_DATA(attr), value.c_str(), value.size() + 1);
        }
        else if constexpr (std::is_integral_v<T>) {
            size = RTA_LENGTH(sizeof(T));
            add();
            std::memcpy(RTA_DATA(attr), &value, sizeof(value));
        }
        else if constexpr (std::is_empty_v<T>) {
            size = RTA_LENGTH(0);
            add();
        }
        else
            static_assert(false, "Don't know how to convert");

        msg->nlmsg_len = NLMSG_ALIGN(msg->nlmsg_len) + RTA_ALIGN(size);
        return attr;
    }

private:
    void setOptsMakeChecks(const int &fd,
                           void * sndBuf, const size_t &sndBufSize,
                           void * rcvBuf, const size_t &rcvBufSize,
                           sockaddr_nl *addr, const size_t &addrSize);
    void attributeParser (const rtattr * attr, int size, AttrCallback handle);
};
