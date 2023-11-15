#include <iostream>
#include <fstream>

#include "Fallback.hxx"

using namespace std;
namespace fs = std::filesystem;

void Fallback::show (std::span<const std::string> bridges)
{
    cout << "Fallback::show() would be implemented sometime" << endl;
}

void Fallback::addbr (const std::string &bridge)
{
    cout << "Fallback::addbr() would be implemented sometime" << endl;
}

void Fallback::delbr (const std::string &bridge)
{
    cout << "Fallback::delbr() would be implemented sometime" << endl;
}

void Fallback::addif (const std::string &bridge, const std::string &device)
{
    cout << "Fallback::addif() would be implemented sometime" << endl;
}

void Fallback::delif (const std::string &bridge, const std::string &device)
{
    cout << "Fallback::delif() would be implemented sometime" << endl;
}

void Fallback::getDevicesAndBridges ()
{
    for (const auto &entry : fs::directory_iterator("/sys/class/net")) {
        if (entry.is_directory()) {
            const fs::path bridgeProps = entry.path() / "bridge";

            if (fs::exists(bridgeProps) && fs::is_directory(bridgeProps))
                _bridges[fs::path(entry).filename()] = getBridge(entry);
            else
                _devices[fs::path(entry).filename()] = getDevice(entry);
        }
    }
}

Device Fallback::getDevice (const fs::directory_entry &dir)
{
    Device dev (fs::path(dir).filename());

    if (fs::is_regular_file(dir.path() / "ifindex"))
        dev.index = readFile<decltype(dev.index)>(
            dir.path() / "ifindex");

    if (fs::is_regular_file(dir.path() / "operstate"))
        dev.operstate = readFile<decltype(dev.operstate)>(
            dir.path() / "operstate");

    if (! dev.isSane())
        throw runtime_error(format("Failed to get interface properties from {}",
                                   dir.path().string()));
    return dev;
}

Bridge Fallback::getBridge (const fs::directory_entry &dir)
{
    Bridge br (getDevice(dir));
    const auto propDir = dir.path() / "bridge";

    if (fs::is_regular_file(propDir / "bridge_id"))
        br.bridge_id = readFile<decltype(br.bridge_id)>(propDir / "bridge_id");

    if (fs::is_regular_file(propDir / "stp_state"))
        br.stp_state = readFile<decltype(br.stp_state)>(propDir / "stp_state");

    if (! br.isSane())
        throw runtime_error(format("Failed to get bridge properties from {}",
                                   dir.path().string()));

    return br;
}

template<class T>
T Fallback::readFile(const fs::path &path)
{
    ifstream is (path.string());
    if (! is.good() )
        throw runtime_error(format("Failed to get information from {}",
                                   path.string()));

    stringstream ss;
    ss << is.rdbuf();
    string tmp;
    getline(ss, tmp); // strip string

    if constexpr (is_same_v<string, T>)
        return tmp;
    else if constexpr (is_same_v<int, T>)
        return stoi(tmp);
    else if constexpr (is_same_v<bool, T>)
        return tmp == "1";
    else
        static_assert(false, "Don't know how to convert");
}
