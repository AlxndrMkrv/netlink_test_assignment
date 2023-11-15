#include "Application.hxx"

#include <iostream>
#include <format>

using namespace std;

static struct __Helper {
    static constexpr string_view helpFmt = "\t{: <10}{: <20}{}";
    static constexpr string_view incorrectNA =
        "Incorrect number of arguments for command";
    static constexpr unsigned int CommandsNumber = 5;

    struct Cmd {
        string_view command;
        string_view args;
        string_view help;
    } commands [CommandsNumber] = {
        {"show", "[<bridge>]", "show a list of bridges"},
        {"addbr", "<bridge>", "add bridge"},
        {"delbr", "<bridge>", "delete bridge"},
        {"addif", "<bridge> <device>", "add interface to bridge"},
        {"delif", "<bridge> <device>", "delete interface from bridge"}
    };

    const Cmd & getCommand(string_view cmd) const {
        for (auto &c : commands) {
            if (c.command == cmd)
                return c;
        }
        throw out_of_range(format("Failed to find \"{}\" command", cmd));
    }

    auto getHelp(const Cmd &c) const {
        return format(helpFmt, c.command, c.args, c.help);
    }

    auto getHelp(string_view cmd) const {
        return getHelp(getCommand(cmd));
    }

    auto getCorrectUsage(const Cmd &c) const {
        return format("{} {} {}", Application::name, c.command, c.args);
    }

    auto getCorrectUsage(string_view cmd) const {
        return getCorrectUsage(getCommand(cmd));
    }

} helper;

void Application::PrintHelp()
{
    cout << format("Usage: {} [commands]", name) << endl
         << "commands:" << endl;
    for (__Helper::Cmd &cmd : span(helper.commands))
        cout << helper.getHelp(cmd) << endl;
}

void Application::run(span<const string> args)
{
    getDevicesAndBridges();

    const auto cmd = args.front();
    bool invalidArgumentsNumber = false;

    if (cmd == "show") {
        show(args.last(args.size() - 1));
    }
    else if (cmd == "addbr") {
        if (args.size() > 1)
            addbr(args[1]);
        else
            invalidArgumentsNumber = true;
    }
    else if (cmd == "delbr") {
        if (args.size() > 1)
            delbr(args[1]);
        else
            invalidArgumentsNumber = true;
    }
    else if (cmd == "addif") {
        if (args.size() > 2)
            addif(args[1], args[2]);
        else
            invalidArgumentsNumber = true;
    }
    else if (cmd == "delif") {
        if (args.size() > 2)
            delif(args[1], args[2]);
        else
            invalidArgumentsNumber = true;
    }
    else {
        if (args.size())
            cout << format("never heard of command [{}]", cmd) << endl;
        PrintHelp();
    }

    if (invalidArgumentsNumber) {
        cout << helper.incorrectNA << endl;
        cout << "Usage: " << helper.getCorrectUsage(cmd) << endl;
    }
}

bool ApplicationData::isMaster (const std::string &dev,
                                const std::string &br) const
{
    auto range = _relations.equal_range(br);
    for (auto it = range.first; it != range.second; ++it)
        if (it->second == dev)
            return true;
    return false;
}
