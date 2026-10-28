# lv_i18n runtime

Lookup runtime of [lv_i18n](https://github.com/lvgl/lv_i18n) 0.2.1, MIT (`LICENSE`).

Upstream is a Node.js CLI whose C runtime exists only as a text template inside
`lib/compiler_template.js`, so there is no standalone C file to pin as a submodule; this directory is a plain vendored copy. `lv_i18n.h` and
`lv_i18n.c` are that template's runtime half, taken from the output of the 0.2.1 CLI. The generated
language data is not vendored: `tools/generate_i18n.py` emits it per Product using the same
structures (`lv_i18n_lang_t`, `lv_i18n_phrase_t`, `lv_i18n_language_pack_t`), so a table produced by
the upstream CLI is accepted as well.

Local changes (keep this list complete when updating):

- `lv_i18n.h`: `singulars` and `plurals[]` point to `const lv_i18n_phrase_t`, so generated tables
  live in flash. The global `lv_i18n_language_pack` and the `_()` / `_p()` macros are dropped; the
  framework passes the Product's pack to `lv_i18n_init()` itself.
- `lv_i18n.c`: only the runtime half; `__lv_i18n_get_text_core()` takes a const table.

Update policy: re-extract from a newer CLI output only when a shipped language needs a plural rule
or API the current runtime lacks, and re-run the framework i18n tests afterwards.
