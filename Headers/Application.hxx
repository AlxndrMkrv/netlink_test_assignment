#pragma once

#include <string_view>
#include <unordered_map>
#include <map>
#include <span>
#include "Device.hxx"

/* Interface are splitted to avoid diamond inheritance in Netlink class */

class Application
{
public:
    static constexpr std::string_view name = "brctl";

public:
    static void PrintHelp ();

    void run (std::span<const std::string> args);

    /* Interface to implement */
    virtual void show (std::span<const std::string> bridges) = 0;
    virtual void addbr (const std::string &bridge) = 0;
    virtual void delbr (const std::string &bridge) = 0;
    virtual void addif (const std::string &bridge,
                        const std::string &device) = 0;
    virtual void delif (const std::string &bridge,
                        const std::string &device) = 0;

protected:
    virtual void getDevicesAndBridges () = 0;
};


class ApplicationData
{
protected:
    bool isMaster (const std::string &dev, const std::string &br) const;

protected:
    // TODO: try to reimplement _devices and _bridges as unordered_set
    // with Key and Hash based on Device::name and find()-> instead of operator[]
    std::unordered_map<std::string, Device> _devices;
    std::map<std::string, Bridge> _bridges;

    // bridge <-> device
    std::unordered_multimap<std::string, std::string> _relations;
};
