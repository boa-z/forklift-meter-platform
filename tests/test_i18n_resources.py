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
