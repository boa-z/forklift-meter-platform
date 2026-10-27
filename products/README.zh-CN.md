# Products

> [English](README.md)

每个 Product 都是克隆到 `products/<id>` 的独立 Git 仓库。Framework 忽略本目录下除这两个 README 以外的全部内容，公开与私有 Product 可以并存，Framework 不记录其中任何一个。

```text
git clone https://github.com/boa-z/forklift-meter-platform-demo.git products/demo
git clone PRIVATE_PRODUCT_REPOSITORY products/my-product
```

Host CMake 没有默认 Product，用 `-DMETER_PRODUCT_ROOT=products/<id>` 选择。固件构建在 defconfig 中用 `CONFIG_AIC_FORKLIFT_PRODUCT_ROOT` 指定 Product。参见[构建指南](../docs/build/build.zh-CN.md)与[下游指南](../docs/product/downstream.zh-CN.md)。
