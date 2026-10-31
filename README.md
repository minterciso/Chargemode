# Chargemode

Battery charge-mode switcher for Linux laptops, with a Qt6 system-tray front end.

It writes the kernel's `charge_types` power-supply attribute (`Standard`, `Fast`, `Long_Life`) and, for the gaming mode, also switches the power-profiles-daemon profile.

## Requirements

- A laptop whose firmware exposes `/sys/class/power_supply/BAT*/charge_types`. Without that node the helper exits with "no charge_types node".
- power-profiles-daemon (reached through `busctl`) and `sudo`.
- Fedora packages to build: `sudo dnf install cmake gcc-c++ qt6-qtbase-devel`

## Components

- `scripts/chargemode`: root-only helper that writes the sysfs node. Subcommands: `gaming`, `standard`, `fast`, `evening`, `status`, `version`.
  - `gaming` sets `Long_Life`, `standard` sets `Standard`, `fast` sets `Fast`.
  - `evening` switches `Fast` back to `Standard` and does nothing otherwise. It is meant to be run from a timer; none is shipped.
- `chargetray/`: Qt6 Widgets tray app. It polls the charge type, power profile and AC state every 10 seconds and shows them in the tooltip.
  - Gaming: `Long_Life` plus the `performance` profile. The profile change is skipped when not on AC.
  - Fast charge and Slow charge (Standard): the `balanced` profile.
  - The helper is called as `sudo -n /usr/local/bin/chargemode`; that path is hardcoded.

## Install the helper

The helper must be owned by root, because the sudoers rule below lets your user run it without a password. If it were writable by you, that rule would be a root escalation.

    sudo install -o root -g root -m 755 scripts/chargemode /usr/local/bin/chargemode
    echo "$USER ALL=(root) NOPASSWD: /usr/local/bin/chargemode" | sudo tee /etc/sudoers.d/chargemode >/dev/null
    sudo chmod 440 /etc/sudoers.d/chargemode
    sudo visudo -cf /etc/sudoers.d/chargemode

Usage:

    chargemode status
    chargemode version
    chargemode gaming

## Build, install and autostart the tray

    cd chargetray
    cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build
    install -Dm755 build/chargetray ~/.local/bin/chargetray
    mkdir -p ~/.config/autostart
    cat > ~/.config/autostart/chargetray.desktop <<EOT
    [Desktop Entry]
    Type=Application
    Name=Charge Tray
    Exec=$HOME/.local/bin/chargetray
    EOT
    ~/.local/bin/chargetray &

## Tests

    cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build
    ctest --test-dir build --output-on-failure

Run these from `chargetray/`. The suite covers `runAsync`, the non-blocking process runner the tray uses. It is built only when Qt6 Test is found.

## Versions

The tray is at v1.1.0 and the helper at v1.0.0; they are versioned independently. `chargetray --version` reads it from `project(... VERSION ...)` in `chargetray/CMakeLists.txt`; `chargemode version` reads the `VERSION` variable at the top of `scripts/chargemode`. Bump both when the interface between them changes (the helper subcommands the tray calls).

## License

GPLv3, see `LICENSE`.
