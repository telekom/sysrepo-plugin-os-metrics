# DT Metrics Plugin

A sysrepo metrics module and implementation to get more information out of Linux through the NETCONF pipeline.

## Building

The plugin is built as a shared library using the [MESON build system](https://mesonbuild.com/) and is required for building the plugin.

```bash
apt install meson ninja-build cmake pkg-config
```

The plugin install location is the `{prefix}` folder, the `libdt-metrics-plugin.so` should be installed over to the `plugins` directory from the sysrepo installation (e.g. the sysrepo install location for the bash commands below is `/opt/sysrepo`)\
Build by making a build directory (i.e. build/), run meson in that dir, and then use ninja to build the desired target.

```bash
mkdir -p /opt/sysrepo/lib/sysrepo/plugins
meson --prefix="/opt/sysrepo/lib/sysrepo/plugins" ./build
ninja -C ./build
```

## Installing

Meson installs the shared-library in the `{prefix}` directory.

```bash
ninja install -C ./build
```

## Running and testing the plugin
The necessary .yang files need to be installed in sysrepo from the `yang` directory

```bash
sysrepoctl -i yang/dt-metrics.yang
sysrepoctl -c dt-metrics -e usage-notifications
```

The sysrepo plugin daemon needs to be loaded after the plugin is installed:

```bash
sysrepo-plugind -v 4 -d
```

The functionality can be tested by doing an operational data request through sysrepo:

```bash
sysrepocfg -x "/dt-metrics:system-metrics" -X -d operational -f json
```

As described in the module itself the plugin holds configuration data provided by the user in the running data-store and uses the data to create and monitor threshold values for filesystem and memory nodes. These configuration nodes and the notifications themselves are grouped under the 'usage-notifications' feature which needs to be enabled.

```bash
sysrepocfg -Iyang/share/dt-metrics-text-config.xml -d running
```

### Dependencies
```
libyang
libyang-cpp
sysrepo
sysrepo-cpp
libprocps
pthreads
df
```

The libyang and sysrepo dependencies should be compiled from their public repositories `libyang1` branches.
The plugin assumes it's being installed on a Debian system and uses tools like `df`, and the `/proc` structure internally.
