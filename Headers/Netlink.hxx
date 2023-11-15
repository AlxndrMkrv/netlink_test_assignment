#pragma once
#include "Application.hxx"
#include "_NetlinkImpl.hxx"

class Netlink : public _NetlinkImpl, public Application
{
public:
    /* Application methods implementation */
    virtual void show (std::span<const std::string> bridges) override;
    virtual void addbr (const std::string &bridge) override;
    virtual void delbr (const std::string &bridge) override;
    virtual void addif (const std::string &bridge,
                        const std::string &device) override;
    virtual void delif (const std::string &bridge,
                        const std::string &device) override;

    // Check if netlink works
    bool check();

protected:
    /* Application methods */
    virtual void getDevicesAndBridges() override;
};
