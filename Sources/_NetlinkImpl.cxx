#include "_NetlinkImpl.hxx"

static const char * oper_states []
    { "UNKNOWN", "NOTPRESENT", "DOWN", "LOWERLAYERDOWN",
      "TESTING", "DORMANT", "UP"};

/* Netlink RTM_NEWLINK message has following structure:
 * +----------+-----------+--------+------+--------+------+-----+
 * | nlmsghdr | ifinfohdr | rtattr | data | rtattr | data | ... |
 * +----------+-----------+--------+------+--------+------+-----+
 * 'data' here may be either a plain like uint32 or char * or even empty,
 * but also be a container for a list of own attributes:
 * +--------+------+--------+------+-----+--------+------+
 * | rtattr | data | rtattr | data | ... | rtattr | data |
 * +--------+------+--------+------+-----+--------+------+
 * Of course these nested attributes may also have a nested attributes and so on
 */

/* The method creates a connection with kernel, sends a request,
 * checks the reply and calls handle for every nlmsghdr */

ErrorCode _NetlinkImpl::talkWithKernel (Message::LinkRequest &rq,
                                        ErrCallback errHandle,
                                        MsgCallback msgHandle)
{
    Socket sock;
    sockaddr_nl address {.nl_family = AF_NETLINK};
    iovec iov {.iov_base = &rq.hdr, .iov_len = rq.hdr.nlmsg_len};
    msghdr msg {.msg_name = &address, .msg_namelen = sizeof(address),
                .msg_iov = &iov, .msg_iovlen = 1};
    ErrorCode errorCode = ErrorCode::Success;

    // Set receive buffer as recommended by docs.kernel.org
    const size_t receiveBufferSize = std::max(8192, getpagesize());
    unsigned char rcvBuffer [receiveBufferSize];

    setOptsMakeChecks(sock.fd(),
                      &rq.buffer, sizeof(rq.buffer),
                      &rcvBuffer, sizeof(rcvBuffer),
                      &address, sizeof(address));


    // don't forget to add RTNL_HANDLE_F_STRICT_CHK
    // it's stored in rth->flags in iproute2

    const int bytesSend = sendmsg(sock.fd(), &msg, 0);
    if (bytesSend == -1)
        throw std::runtime_error(std::format("Failed to sendmsg(), {}",
                                             std::strerror(errno)));
    else if (bytesSend != iov.iov_len)
        throw std::runtime_error(std::format("Incorrect sendmsg() bytes: "
                                             "sent {}, must be {}",
                                             bytesSend, iov.iov_len));

    iov.iov_base = &rcvBuffer;

    bool complete = false;
    while (! complete) {
        iov.iov_len = sizeof(rcvBuffer);

        int bytesReceived = recvmsg(sock.fd(), &msg, 0);
        if (bytesReceived < 0) {
            if (errno == EINTR || errno == EAGAIN)
                continue;
            throw std::runtime_error(std::format("Failed to recvmsg(), {}",
                                                 std::strerror(errno)));
        }

        if (msg.msg_flags & MSG_TRUNC)
            throw std::runtime_error("Truncated message");

        nlmsghdr * hdr = reinterpret_cast<nlmsghdr *>(rcvBuffer);

        while (NLMSG_OK(hdr, bytesReceived)) {
            int messageSize = hdr->nlmsg_len;
            int dataSize = messageSize - sizeof(*hdr);

            // Check sizes
            if (dataSize < 0 || messageSize > bytesReceived)
                throw std::runtime_error(std::format("Invalid message size {}",
                                                     messageSize));

            // If NLM_F_DUMP_INTR presend the dump must be reasked
            if (hdr->nlmsg_flags & NLM_F_DUMP_INTR)
                errorCode = ErrorCode::DumpInconsistent;

            // Pass NLMSG_ERROR to handler if possible and go on
            if (hdr->nlmsg_type == NLMSG_ERROR) {
                if (errHandle != nullptr)
                    errHandle(reinterpret_cast<nlmsgerr *>(NLMSG_DATA(hdr)));
                // Stop if no more data
                if (messageSize == bytesReceived)
                    complete = true;
            }
            // Stop on NMLSG_DONE
            else if (hdr->nlmsg_type == NLMSG_DONE) {
                if (hdr->nlmsg_flags & NLM_F_MULTI) {
                    complete = true;
                    break;
                } else
                    throw std::runtime_error("Got NLMSG_DONE without "
                                             "NLM_F_MULTI flag");
            }
            // Pass everything else to handler if possible
            else if (msgHandle != nullptr)
                    msgHandle(hdr);

            // Pick the next header
            hdr = NLMSG_NEXT(hdr, bytesReceived);
        }
    }
    return errorCode;
}

std::string_view _NetlinkImpl::getOperstate(const rtattr * const attr) const
{
    return oper_states[readAttr<uint8_t>(attr)];
}

std::string _NetlinkImpl::getBridgeId(const rtattr * const attr) const
{
    ifla_bridge_id *id = readAttr<ifla_bridge_id*>(attr);
    return std::format("{:02x}{:02x}.{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}",
                       id->prio[0], id->prio[1],
                       id->addr[0], id->addr[1], id->addr[2],
                       id->addr[3], id->addr[4], id->addr[5]);
}

void _NetlinkImpl::parseAttrs(const nlmsghdr * const hdr, AttrCallback handle)
{
    ifinfomsg * ifi = reinterpret_cast<ifinfomsg *>(NLMSG_DATA(hdr));
    rtattr *attr = IFLA_RTA(ifi);
    const int size = hdr->nlmsg_len - NLMSG_LENGTH(sizeof(*ifi));
    attributeParser(attr, size, handle);
}

void _NetlinkImpl::parseNestedAttrs (const rtattr *const parent,
                                     AttrCallback handle)
{
    rtattr *child = reinterpret_cast<rtattr *>(RTA_DATA(parent));
    attributeParser(child, RTA_PAYLOAD(parent), handle);
}

void _NetlinkImpl::setOptsMakeChecks(const int &fd,
                                     void * sndBuf, const size_t &sndBufSize,
                                     void * rcvBuf, const size_t &rcvBufSize,
                                     sockaddr_nl *addr, const size_t &addrSize)
{
    int one = 1;

    if (setsockopt(fd, SOL_SOCKET, SO_SNDBUF, sndBuf, sndBufSize) < 0)
        throw std::runtime_error("Failed to set SO_SNDBUF");

    if (setsockopt(fd, SOL_SOCKET, SO_RCVBUF, rcvBuf, rcvBufSize) < 0)
        throw std::runtime_error("Failed to set SO_RCVBUF");

    if (setsockopt(fd, SOL_NETLINK, NETLINK_GET_STRICT_CHK,
                   &one, sizeof(one)) < 0)
        throw std::runtime_error("Failed to set NETLINK_GET_STRICT_CHK");

    if (bind(fd, reinterpret_cast<sockaddr *>(addr), addrSize) < 0)
        throw std::runtime_error("Failed to bind socket");

    size_t chkAddrSize = 0;

    if (getsockname(fd, reinterpret_cast<sockaddr *>(addr),
                    reinterpret_cast<socklen_t *>(&chkAddrSize)) < 0)
        throw std::runtime_error("Failed to getsockname()");

    if (addrSize != chkAddrSize)
        throw std::runtime_error(std::format("Got invalid address length {}",
                                             chkAddrSize));

    if (addr->nl_family != AF_NETLINK)
        throw std::runtime_error(std::format("Got invalid address family {}",
                                             addr->nl_family));
}

void _NetlinkImpl::attributeParser(const rtattr * attr, int size,
                                   AttrCallback handle)
{
    unsigned short type;
    while (RTA_OK(attr, size)) {
        type = attr->rta_type & ~NLA_F_NESTED;
        handle(type, attr);
        attr = RTA_NEXT(attr, size);
    }
    if (size)
        throw std::runtime_error(
            std::format("{} bytes left after attribute parse", size));
}
