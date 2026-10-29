"""资源导入拒绝重复键，避免 JSON 解析静默覆盖译文或语言编号。"""
import importlib.util
from pathlib import Path

import pytest

spec = importlib.util.spec_from_file_location(
    'i18n_generator', Path(__file__).resolve().parents[1] / 'tools/generate_i18n.py')
i18n = importlib.util.module_from_spec(spec)
spec.loader.exec_module(i18n)


@pytest.mark.parametrize('content', [
    '{"label":"first", "label":"second"}',
    '{"languages":[{"id":0,"id":1,"code":"en"}]}',
])
def test_duplicate_resource_keys_are_rejected(tmp_path, content):
    resource = tmp_path / 'resource.json'
    resource.write_text(content, encoding='utf-8')
    with pytest.raises(i18n.I18nError, match='重复 JSON 键'):
        i18n.load_json(resource)


def test_same_key_in_distinct_language_objects_is_valid(tmp_path):
    resource = tmp_path / 'resource.json'
    resource.write_text('{"languages":[{"id":0},{"id":1}]}', encoding='utf-8')
    assert i18n.load_json(resource) == {'languages': [{'id': 0}, {'id': 1}]}


def write_product(root, symbol='product_i18n'):
    (root / 'i18n/translations').mkdir(parents=True)
    (root / 'i18n/languages.json').write_text(json_text({
        'symbol': symbol, 'source': 'en', 'default': 'en',
        'languages': [
            {'id': 0, 'code': 'zh-CN', 'plural': 'other_only'},
            {'id': 1, 'code': 'en', 'plural': 'one_other'}]}), encoding='utf-8')
    source = {'ui.title': 'Title', 'ui.count.one': '{0} item', 'ui.count.other': '{0} items', 'ui.only': 'Plain'}
    chinese = {'ui.title': '标题', 'ui.count.one': '{0} 项', 'ui.count.other': '{0} 项', 'ui.only': '普通'}
    (root / 'i18n/translations/en.json').write_text(json_text(source), encoding='utf-8')
    (root / 'i18n/translations/zh-CN.json').write_text(json_text(chinese), encoding='utf-8')
    return root / 'i18n/generated'


def json_text(value):
    import json
    return json.dumps(value, ensure_ascii=False)


def test_generated_pack_is_an_lv_i18n_table(tmp_path):
    out = write_product(tmp_path)
    assert i18n.main(['--product-root', str(tmp_path)]) == 0
    source = (out / 'product_i18n.c').read_text(encoding='utf-8')
    header = (out / 'product_i18n.h').read_text(encoding='utf-8')
    # 源语言排第一，作为 lv_i18n 的回退语言；语言表仍按持久化编号排列。
    pack = source[source.index('product_i18n_pack['):]
    assert pack.index('&en_lang') < pack.index('&zh_cn_lang')
    assert 'extern const lv_i18n_language_pack_t product_i18n_pack[PRODUCT_I18N_LANGUAGE_COUNT + 1u];' in header
    assert '{(hmi_language_t)0, "zh-CN", NULL, NULL}' in source
    # <键>.one / <键>.other 成对的消息进入复数表，其余留在单数表；.one/.other 不再是单数键。
    assert '{"ui.count", "{0} item"}' in source and '{"ui.count", "{0} items"}' in source
    assert '{"ui.count.one"' not in source and '{"ui.title", "Title"}' in source
    assert '.plurals = {[LV_I18N_PLURAL_TYPE_ONE] = en_plural_one, [LV_I18N_PLURAL_TYPE_OTHER] = en_plural_other}' in source
    assert 'return n == 1 ? LV_I18N_PLURAL_TYPE_ONE : LV_I18N_PLURAL_TYPE_OTHER;' in source
    # 过期检查：改一条译文而不重新生成，--check 必须失败。
    assert i18n.main(['--product-root', str(tmp_path), '--check']) == 0
    (tmp_path / 'i18n/translations/en.json').write_text(json_text({
        'ui.title': 'Changed', 'ui.count.one': '{0} item', 'ui.count.other': '{0} items', 'ui.only': 'Plain'}),
        encoding='utf-8')
    assert i18n.main(['--product-root', str(tmp_path), '--check']) == 1
