# Products

> [中文版](README.zh-CN.md)

Each Product is an independent Git repository cloned into `products/<id>`. The Framework ignores everything in this directory except these two README files, so public and private Products sit side by side without the Framework recording any of them.

```text
git clone https://github.com/boa-z/forklift-meter-platform-demo.git products/demo
git clone PRIVATE_PRODUCT_REPOSITORY products/my-product
```

Host CMake has no default Product; select one with `-DHMI_PRODUCT_ROOT=products/<id>`. A firmware build names its Product with `CONFIG_AIC_HMI_PRODUCT_ROOT` in the defconfig. See the [build guide](../docs/build/build.md) and the [downstream guide](../docs/product/downstream.md).
