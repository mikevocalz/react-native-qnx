# Linux/Wayland MarathonOS adapter

The shared React Native Fabric/Hermes/Qt Quick host is in vnext, not duplicated here. The experimental Linux Marathon target compiles vnext/probes/playground/main.cpp with RNQNX_MARATHON_HOST=1 and uses the existing rnqnx_react_host. It has no QNX Screen dependency.

This directory is reserved for MarathonOS **specific** system services once the upstream contracts are agreed: shell lifecycle and back, keyboard/IME, safe areas/insets, Wayland app identity, permissioned D-Bus, background tasks, audio/sensors/network, signing/packaging. These are not implemented by this first executable slice.

Do NOT use direct unrestricted D-Bus or turn off bwrap as a workaround. Do NOT call this an official .marathon app merely because a native Wayland window opens.

See docs/marathon/NATIVE_HOST.md and upstream docs/SECURITY_MODEL.md, docs/NATIVE_APP_LIFECYCLE.md, docs/APP_MANIFEST_SCHEMA.md.
