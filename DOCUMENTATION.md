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
