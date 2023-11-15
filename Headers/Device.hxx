#pragma once
#include <string>
#include <format>


struct Device
{
    std::string name;
    std::string operstate;
    int index;

public:
    bool isSane () const {
        return name.length() && index;
    }

    operator std::string() {
        return std::format("{}: {}: {}", index, name, operstate);
    }
};

struct Bridge : public Device
{
    std::string bridge_id;
    bool stp_state;

public:
    constexpr Bridge() : Device{"", "", 0}, bridge_id{""}, stp_state(false) {}

    Bridge(const Device &dev) :
        Device{dev} {}

    bool isSane () const {
        return Device::isSane() && bridge_id.length();
    }

    operator std::string() {
        return std::format("{}: {}: {} STP={} {}",
                           index, name, bridge_id,
                           (stp_state ? "ON" : "OFF"), operstate);
    }
};

