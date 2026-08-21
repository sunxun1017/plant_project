---
name: plant-config-layer
description: Maintain Plant build profiles and product configuration inputs in 10_config plus sdkconfig and partition defaults. Use when changing development/test/production profiles, ESP32-C3 Kconfig, partitions, feature gates, security settings, or persisted/default limits.
---

# Configuration Layer Boundary

`10_config` separates development, test, production, and default profiles. Root `sdkconfig.defaults`, `partitions.csv`, and applicable build inputs are versioned product configuration and must remain reproducible.

## Owns

- Feature/profile selection, safe defaults, target Kconfig, partition layout, and compile-time security/power options.

## Must not own

- Product behavior algorithms, vendor calls, runtime state, duplicated GPIO mappings, secrets, private keys, or machine-generated full sdkconfig files unless the project explicitly chooses to version one.

Development relaxation must be explicit and must not silently weaken production signing, downgrade, BLE authentication, or rollback policy. Keep credentials and signing private keys outside Git.

For changes, perform a fresh ESP32-C3 configure/build from defaults, inspect the generated Kconfig values and partition table, check binary headroom, and update product/testing documentation when acceptance or provisioning changes.
