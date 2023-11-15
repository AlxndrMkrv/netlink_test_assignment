#include <iostream>

#include "Netlink.hxx"
#include "Fallback.hxx"

int main (int argc, const char * argv [])
{
    std::vector<std::string> args (argv, argv + argc);
    const bool useFallback = args.size() > 1 && args[1] == "-fb" ? true : false;

    if (args.size() == 1)
        Application::PrintHelp();

    else {
        const auto argsToPass = args.size() - (useFallback ? 2 : 1);

        try {
            Netlink nl;
            // Run Netlink version if Fallback flag not set and check() passes
            if (! useFallback && nl.check())
                nl.run(std::span(args.data(), args.size()).last(argsToPass));
            // Run ioctl/sysfs version otherwise
            else {
                Fallback fb;
                fb.run(std::span(args.data(), args.size()).last(argsToPass));
            }
        } catch (std::exception &e) {
            std::cout << e.what() << std::endl;
        }
    }

    return 0;
}
