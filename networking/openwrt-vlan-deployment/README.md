# OpenWrt VLAN Deployment Study

**Type:** Network configuration study  
**Stage:** Deployment documentation

A sanitized technical study of VLAN separation, upstream management, tagged/untagged ports, and Wi-Fi service separation on OpenWrt-class router hardware.

## Topics Covered

- management network separation;
- service VLAN design;
- access, trunk, and hybrid port concepts;
- upstream DHCP management;
- administrative and service SSID separation;
- maintenance access and package cleanup.

## Public Repository Policy

Raw router backups are deliberately excluded because real backup archives can contain Wi-Fi credentials, password database material, private SSH keys, TLS keys, MAC-specific values, and deployment addresses.

Any reusable public configuration should be built from templates with placeholders rather than copied from a live router.
