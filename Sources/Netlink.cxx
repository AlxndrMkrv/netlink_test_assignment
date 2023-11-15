#include <linux/netlink.h>
#include <sstream>
#include <iostream>

#include "Netlink.hxx"
#include "Request.hxx"

void Netlink::show (std::span<const std::string> bridges)
{
    bool headerPrinted = false;

    auto printBridge = [&](const std::string &iface) {
        if (! _devices.contains(iface) && ! _bridges.contains(iface))
            std::cout << std::format("bridge {} does not exist!", iface)
                      << std::endl;
        else if (! _bridges.contains(iface))
            std::cout << std::format("device {} is not a bridge!", iface)
                      << std::endl;
        else if (_bridges.contains(iface)) {
            if (! headerPrinted) {
                std::cout << std::format("{}\t{}\t\t{}\t{}",
                                         "bridge name", "bridge id",
                                         "STP enabled", "interfaces")
                          << std::endl;
                headerPrinted = true;
            }
            auto masters = [&](const std::string &br){
                std::stringstream sstr;
                int counter = _relations.count(br);
                auto range = _relations.equal_range(br);
                for (auto it = range.first; it != range.second; ++it)
                    sstr << it->second << (--counter ? ", " : "");
                return sstr.str();
            };
            std::cout << std::format("{}\t\t{}\t{}\t\t{}",
                                     iface, _bridges[iface].bridge_id,
                                     _bridges[iface].stp_state ? "yes" : "no",
                                     masters(iface))
                      << std::endl;
        }
    };

    // If some parameters to 'show' given
    if (bridges.size())
        for (const auto &iface : bridges)
            printBridge(iface);

    // Print all bridges if 'show' is bare
    else if (_bridges.size())
        for (const auto &iface : _bridges)
            printBridge(iface.first);
}

void Netlink::addbr (const std::string &bridge)
{
    if (_bridges.count(bridge) || _devices.count(bridge))
        throw std::runtime_error(
            std::format("device {} already exists; can't create bridge with "
                        "the same name", bridge));

    Message::LinkRequest request (RTM_NEWLINK,
                                  NLM_F_REQUEST | NLM_F_CREATE |
                                     NLM_F_EXCL | NLM_F_ACK);
    addAttr<std::string, decltype(request)>(&request.hdr, IFLA_IFNAME, bridge);

    // Add empty attr IFLA_LINKINFO with nested attr IFLA_INFO_KIND
    rtattr * linkInfoAttr =
        addAttr<EmptyAttr, decltype(request)>(&request.hdr, IFLA_LINKINFO);
    rtattr * attr = addAttr<std::string, decltype(request)>(&request.hdr,
                                                            IFLA_INFO_KIND,
                                                            "bridge");
    --attr->rta_len; // somehow IFLA_INFO_KIND shouldn't contain '\0'

    // Calculate IFLA_LINKINFO size
    linkInfoAttr->rta_len = NLMSG_NESTED_RTA_SIZE(&request.hdr, linkInfoAttr);

    auto errHandler = [&](nlmsgerr *err) {
        if (err->error)
            throw std::runtime_error(
                std::format("add bridge failed: {}",
                            std::strerror(-err->error)));
    };

    talkWithKernel(request, errHandler);
}

void Netlink::delbr (const std::string &bridge)
{
    if (! _bridges.contains(bridge))
        throw std::runtime_error(
            std::format("bridge {} doesn't exist; can't delete it", bridge));

    Message::LinkRequest request (RTM_DELLINK,
                                  NLM_F_REQUEST | NLM_F_ACK);
    request.ifi.ifi_index = _bridges[bridge].index;
    rtattr * linkInfoAttr =
        addAttr<EmptyAttr, decltype(request)>(&request.hdr, IFLA_LINKINFO);
    rtattr * attr = addAttr<std::string, decltype(request)>(&request.hdr,
                                                            IFLA_INFO_KIND,
                                                            "bridge");
    --attr->rta_len; // IFLA_INFO_KIND shouldn't contain '\0'

    // Calculate IFLA_LINKINFO size
    linkInfoAttr->rta_len = NLMSG_NESTED_RTA_SIZE(&request.hdr, linkInfoAttr);

    auto errHandler = [&](nlmsgerr *err) {
        if (err->error)
            throw std::runtime_error(
                std::format("can't delete bridge {}: {}",
                            bridge, std::strerror(-err->error)));
    };

    talkWithKernel(request, errHandler);
}

void Netlink::addif (const std::string &bridge, const std::string &device)
{
    if (! _devices.contains(device))
        throw std::runtime_error(
            std::format("interface {} doest not exist!", device));

    if (! _bridges.contains(bridge))
        throw std::runtime_error(
            std::format("bridge {} does not exist!", bridge));

    // Send RTM_NEWLINK with eth0 index in ifi and bridge index in IFLA_MASTER
    Message::LinkRequest request (RTM_NEWLINK, NLM_F_REQUEST | NLM_F_ACK);
    request.ifi.ifi_index = _devices[device].index;
    addAttr<uint32_t, decltype(request)>(&request.hdr, IFLA_MASTER,
                                         _bridges[bridge].index);

    auto errHandler = [&](nlmsgerr *err) {
        if (err->error)
            throw std::runtime_error(
                std::format("can't add {} to bridge {}: {}",
                            device, bridge, std::strerror(-err->error)));
    };

    talkWithKernel(request, errHandler);
}

void Netlink::delif (const std::string &bridge, const std::string &device)
{
    if (! _devices.contains(device))
        throw std::runtime_error(
            std::format("interface {} doest not exist!", device));

    if (! _bridges.contains(bridge))
        throw std::runtime_error(
            std::format("bridge {} does not exist!", bridge));

    if (! isMaster(device, bridge))
        throw std::runtime_error(
            std::format("device {} is not a port of {}", device, bridge));

    // Send RTM_NEWLINK with eth0 index in ifi and bridge index in IFLA_MASTER
    Message::LinkRequest request (RTM_NEWLINK, NLM_F_REQUEST | NLM_F_ACK);
    request.ifi.ifi_index = _devices[device].index;
    addAttr<uint32_t, decltype(request)>(&request.hdr, IFLA_MASTER, 0);

    auto errHandler = [&](nlmsgerr *err) {
        if (err->error)
            throw std::runtime_error(
                std::format("can't delete {} from {}: {}",
                            device, bridge, std::strerror(-err->error)));
    };

    talkWithKernel(request, errHandler);
}

/* Check if RTM_NEWLINK available.
 * Not really useful for the assignment. Just a cool stuff from iproute2
 */
bool Netlink::check()
{
    Message::LinkRequest request (RTM_NEWLINK, NLM_F_REQUEST | NLM_F_ACK);

    bool supported = false;
    auto errHandler = [&supported](nlmsgerr *err){
        if (err->error != EOPNOTSUPP && err->error != EINVAL)
            supported = true;
    };

    talkWithKernel(request, errHandler);
    return supported;
}

void Netlink::getDevicesAndBridges ()
{
    // Prepare the request for the kernel
    Message::LinkRequest request (RTM_GETLINK,
                                  NLM_F_REQUEST | NLM_F_ACK | NLM_F_DUMP);
    addAttr<uint32_t, decltype(request)>(&request.hdr, IFLA_EXT_MASK,
                                         RTEXT_FILTER_VF |
                                             RTEXT_FILTER_SKIP_STATS);

    // Create a vector of fields that would be filled from nlmsghdr
    struct Something : public Bridge {
        uint32_t master;
    };
    std::vector<Something> results;

    // Handler to parse BR nested attributes
    auto bridgeAttrHandler = [&](uint16_t type, const rtattr * const attr) {
        if (type == IFLA_BR_BRIDGE_ID)
            results.back().bridge_id = getBridgeId(attr);
        else if (type == IFLA_BR_STP_STATE)
            results.back().stp_state = readAttr<uint32_t>(attr);
    };

    // Handler to parse INFO nested attributes
    auto infoAttrHandler = [&](uint16_t type, const rtattr * const attr) {
        static bool isBridge = false;
        if (type == IFLA_INFO_KIND)
            isBridge = readAttr<std::string_view>(attr) == "bridge";
        else if (type == IFLA_INFO_DATA && isBridge) {
            parseNestedAttrs(attr, bridgeAttrHandler);
            isBridge = false;
        }
    };

    // Handler to parse
    auto attrHandler = [&](uint16_t type, const rtattr * const attr) {
        if (type == IFLA_IFNAME)
            results.back().name = readAttr<std::string_view>(attr);
        else if (type == IFLA_MASTER)
            results.back().master = readAttr<uint32_t>(attr);
        else if (type == IFLA_OPERSTATE)
            results.back().operstate = getOperstate(attr);
        else if (type == IFLA_LINKINFO)
            parseNestedAttrs(attr, infoAttrHandler);
    };

    // Handler to parse Netlink messages
    auto headerHandler = [&](nlmsghdr *hdr){
        if (hdr->nlmsg_type == RTM_NEWLINK) {
            results.push_back(Something{.master = 0});

            ifinfomsg *ifi = reinterpret_cast<ifinfomsg *>(NLMSG_DATA(hdr));
            results.back().index = ifi->ifi_index;

            parseAttrs(hdr, attrHandler);
        }
    };

    talkWithKernel(request, nullptr, headerHandler);

    // Separate flies from cutlets
    for (Something &wtf : results) {
        if (static_cast<Bridge>(wtf).isSane())
            _bridges[wtf.name] = static_cast<Bridge>(wtf);
        else if (static_cast<Device>(wtf).isSane())
            _devices[wtf.name] = static_cast<Device>(wtf);
        else
            throw std::runtime_error(
                std::format("Got something insane "
                            "{}: {}: {} STP {} {}",
                            wtf.index, wtf.name, wtf.operstate, wtf.stp_state,
                            wtf.bridge_id));
    }

    // Trace the relations
    for (Something &wtf : results) {
        if (wtf.master) {
            for (auto &br : _bridges)
                if (wtf.master == br.second.index)
                    _relations.insert({br.first, wtf.name});
        }
    }
}

