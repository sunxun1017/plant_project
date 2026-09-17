# plant_project

ESP32-C3 植物交互设备固件。

- 2：原有业务实现，板级定义是旧的假设基线，不适用于 ZW V1.0 发版板。
- zw_v1：按 ZW V1.0 原理图、BOM 和 PCB 新增的硬件联调目标。

请先阅读 [新板硬件审查](00_docs/hardware/zw_v1_review.md) 和 [联调说明](07_products/plant_zw_v1/README.md)。

新板目标默认不启用电源支路，须先处理审查中明确的 PNP 基极限流问题。它尚未接入旧版 BLE/OTA/生长业务，不是量产固件。

原始需求说明保存在 [original_readme.md](00_docs/product/original_readme.md)，避免 Windows 上 README.md/readme.md 大小写冲突。
