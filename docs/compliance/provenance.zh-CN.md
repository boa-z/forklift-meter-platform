# 依赖与生成器溯源

## 采用代码边界

下表固定版本对应本次检查的构建；capture_toolchain.py 与镜像报告为每次运行保存准确 checkout。Upstream 代码不是第一方合规证据。本阶段不修改 upstream 源码。

| 组件 | 固定提交 / 版本 | 许可 | 边界 / 证据 | 已知限制 |
|---|---|---|---|---|
| RT-Thread / ArtInChip | SDK 9b78386dcaa54326487c8b208b2b7104d4b56c9b | RT-Thread Apache-2.0；SDK 按组件许可 | 原生 IPC、CAN completion、存储驱动；target 构建与单总线 HIL | 不宣称 SDK 统一许可或 MISRA；target 驱动 warning 保持 upstream finding 分类 |
| LVGL | 9.6.0 / 80ca777e37a2b176770726a02e07a6fb79ef0b39 | MIT | 仅 UI owner；Host/target 构建、UI flush 诊断 | 本阶段不替代目视/触摸验收 |
| lvgl-aic | Public 9d8040f28c35b2b3a6339f6971871144d8acedd5；target dfdd4c0c07b6d09a438ca8a0627b3deeb3b0e918 | 被检查 target 副本未找到根许可；待评审 | 采用的 LVGL 板级适配 | Public 与 target pin 不同，不宣称依赖构建一致 |
| CANopenNode | v4.1 / ac2140717c3c498d9b0351bce052bab630a74764 | Apache-2.0 | Adapter 协议引擎与记录 CRC；Host 测试 | 本阶段测试 CAN0 Demo，非物理混合 CANopen 总线 |
| iso14229 | 2e36afcd7f0cd02b0c70446c1a265a7b1999d478 | MIT | UDS/ISO-TP transport；OTA Host 与实板证据 | hash 不是签名；native-auto 确认不是 rollback 验证 |

## 生成生产 C

| 生成器 | 版本身份 | 输入 | 输出 | 门禁 |
|---|---|---|---|---|
| tools/protocol/generate_can.py + cantools | 第一方生成器由记录的 Platform SHA 固定；cantools 40.7.1 | 各 Product protocol/can/*.dbc 和 domain-map.yaml | 各 Product generated/can/*.c 与头文件 | 重生成/replay/compiler，以及 tools/analyze_generated.py 中显式 Cppcheck、clang-tidy |
| tools/generate_catalog.py | 第一方生成器由记录的 Platform SHA 固定 | products/demo/catalog/demo_catalog.json | products/demo/generated/demo_catalog.c 与头文件 | 重生成/compiler，以及显式 Cppcheck、clang-tidy |
| tools/generate_fonts.py + lv_font_conv | 第一方脚本由记录的 Platform SHA 固定；lv_font_conv 1.5.3 | Product 字符/翻译输入与固定字体 | 生成的 C 字体数组 | 归类为资产；字体/许可/大小/compiler 门禁，不作为通用生产逻辑 |

手写 Reference catalog 仍是第一方代码。生成逻辑不因自动生成而排除；finding 应修复生成器再重生成。新增 analyzer 步骤使用 portable C11 和显式 Product includes，不代表 target ABI。原 portable analyzer 步骤及 severity 配置不变，本次仅增加覆盖范围。第一方 portable 与 generated 检查为独立 CI 步骤；upstream 构建诊断仍在编译器日志中可见，不伪装成第一方 finding，这些步骤也不代表 upstream warning 完整清单。

Catalog 信号/参数定义把指针和较宽标量置于 ID 前，消除多余 native padding。旧位置初始化须迁移为具名字段；公开示例与 external 模板均已迁移。这些 struct 不是介质或 CAN 格式，MSP2 仍显式编码稳定 ID 与值。External Product 须使用新头文件重新编译。

## Target 与工具链证据

原生 SCons verbose 命令与 SDK 工作目录一起保存。增量构建只捕获重编译翻译单元，部分命令不是完整 compilation database。本次 target 为 Xuantie GCC 10.2.0（V2.6.1 B-20220906）、newlib 3.2.0，使用 rv32imafdcpzpsfoperand_xtheade / ilp32d、O2、g2、Wall。未输出显式 C 语言选项，采用编译器默认值。Target-specific 第一方静态分析仍是下一门禁。Linux quality artifact 保存实际 GCC/Clang/clang-tidy/Cppcheck 版本、pip freeze 和编译命令；发行版工具版本可能变化，单次 artifact 才是该次运行依据。
