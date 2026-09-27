# Private downstream guide

Start a new private repository from a tagged public platform release and set:

```text
origin   → private product repository
upstream → https://github.com/boa-z/forklift-meter-platform
```

Add customer code only below `products/<customer>`, `protocols/vendor/<customer>`, `ui/products/<customer>` and a private asset/document tree. Add a product composition record and selected source list. Keep customer catalogs, captures, object dictionaries, maintenance objects and UI assets out of the public repository. Do not add customer symbols or conditions to `contracts`, `core`, `runtime`, `protocols/common` or `ui/common` unless a reviewed generic capability is genuinely missing.

For each downstream build record the public platform commit, all submodule SHAs, SDK commit, selected product, image SHA256 and test logs. Public platform changes flow upstream to private products after review; customer code never flows upstream automatically. Private board validation remains a separate D50T-2-Lite reservation with original serial evidence.
