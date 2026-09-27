# Vendored third-party code

| File          | Project | Version | License |
|---------------|---------|---------|---------|
| `cxxopts.hpp` | [cxxopts](https://github.com/jarro2783/cxxopts) | v3.3.1 | MIT |

Source: https://raw.githubusercontent.com/jarro2783/cxxopts/v3.3.1/include/cxxopts.hpp
(sha256 `80c35bb692f44a4cae5fc47f3a2dbe7138fa268f26340f5ab556d8fd07edbb6d`)

Committed as-is rather than fetched at configure time, so a fresh clone builds
with no extra network access. Do not edit it; to upgrade, replace the file and
update this table.

It is added to the include path as a SYSTEM directory, so warnings inside it do
not appear under `-Wall -Wextra -Wpedantic`.
