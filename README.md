## Netlink test assignment

### Objective

The goal of this assignment was to implement `brctl` application acting like [the original one](https://git.kernel.org/pub/scm/network/bridge/bridge-utils.git/) but working with the Netlink communication protocol with following functionality:

``` bash
Usage: brctl [commands]
commands:
        show      [<bridge>]          show a list of bridges
        addbr     <bridge>            add bridge
        delbr     <bridge>            delete bridge
        addif     <bridge> <device>   add interface to bridge
        delif     <bridge> <device>   delete interface from bridge
```


### Build & Run

Just do
``` bash
$ cmake
$ make
```
and have fun. I don't have enough interfaces on my PC to make a good test coverage so would be glad for some feedback on any mistakes you found.

### Complaints

Unfortunately I didn't find a decent Netlink documentation except a brief coverage on [docs.kernel.org](https://docs.kernel.org/userspace-api/netlink/index.html) so most of the code are based on [iproute2](https://github.com/iproute2/iproute2).
