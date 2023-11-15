#pragma once

#include "Application.hxx"
#include <filesystem>

/* Fallback application based on ioctl and sysfs */

class Fallback : public Application, public ApplicationData
{
public:
    virtual void show (std::span<const std::string> bridges) override;
    virtual void addbr (const std::string &bridge) override;
    virtual void delbr (const std::string &bridge) override;
    virtual void addif (const std::string &bridge,
                        const std::string &device) override;
    virtual void delif (const std::string &bridge,
                        const std::string &device) override;

protected:
    virtual void getDevicesAndBridges () override;

private:
    Device getDevice (const std::filesystem::directory_entry &dir);
    Bridge getBridge (const std::filesystem::directory_entry &dir);
    template <class T>
    T readFile (const std::filesystem::path &path);
};
