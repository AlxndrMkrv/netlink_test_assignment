cmake_minimum_required(VERSION 3.1)

set(CMAKE_CXX_STANDARD 20)

project(netlink_test_assignment)

include_directories(Headers)
add_executable(brctl Sources/brctl.cxx
                     Headers/Netlink.hxx
                     Sources/Netlink.cxx
                     Headers/Request.hxx
                     Headers/Socket.hxx
                     Headers/Device.hxx
                     Headers/Fallback.hxx
                     Sources/Fallback.cxx
                     Headers/Application.hxx
                     Sources/Application.cxx
                     Headers/_NetlinkImpl.hxx
                     Sources/_NetlinkImpl.cxx
)
