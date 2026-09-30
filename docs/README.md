# Framework documentation

> [中文版](README.zh-CN.md)

Choose the task you need to complete, then open its module. Operator guides are separated from technical references, historical validation and governance records.

## Start with a task

- [Build the first Product](build/build.md)
- [Run the simulator](build/simulator.md)
- [Build an OTA package and install over CAN](ota/can-update.md)
- [Integrate an independent customer Product](product/downstream.md)
- [Inspect UART and runtime diagnostics](runtime/diagnostics.md)
- [Run CAN hardware tests](testing/can-hil.md)

## Browse by module

| Directory | Contents | Entry |
|---|---|---|
| `build/` | Build and board integration | [Module index](build/README.md) |
| `product/` | Product composition and services | [Module index](product/README.md) |
| `runtime/` | Runtime, protocols and storage | [Module index](runtime/README.md) |
| `ota/` | CAN OTA | [Module index](ota/README.md) |
| `testing/` | Testing and validation | [Module index](testing/README.md) |
| `maintenance/` | Maintenance and documentation | [Module index](maintenance/README.md) |
| `compliance/` | Governance and provenance | [Module index](compliance/README.md) |

## Maintaining documentation

Place new documents in their module rather than directly in this directory. Keep English and Chinese files together, and update module indexes and cross-links in the same change.

Operator guides follow: goal/artifacts → prerequisites → steps and pass criteria → troubleshooting → terms → technical references. State where commands run and which result fields to check. See the [documentation and bilingual rules](maintenance/bilingual-docs.md).

This reorganization preserves test SHAs, image identities and acceptance boundaries. Historical validation does not transfer to later commits.
