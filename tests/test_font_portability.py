"""验证字体生成注释不包含构建主机路径。"""
import importlib.util
from pathlib import Path
import pytest

spec = importlib.util.spec_from_file_location('fonts', Path(__file__).resolve().parents[1] / 'tools/generate_fonts.py')
fonts = importlib.util.module_from_spec(spec)
spec.loader.exec_module(fonts)


def test_font_command_paths_are_portable():
    suffix = ' --size 14 --symbols 你好 --output generated/font.c\n */\nstatic const int value = 42;\n'
    for path in ('C:/Users/Builder/Private Workspace/font.otf',
                 r'C:\Users\Builder\Workspace\font.otf', '/home/builder/private/font.otf'):
        source = '/*\n * Opts: --font ' + path + suffix
        normalized = fonts.portable_font_source(source)
        assert normalized == '/*\n * Opts: --font ' + fonts.FONT + suffix
        assert fonts.portable_font_source(normalized) == normalized

def test_font_sources_include_dynamic_catalogs(tmp_path):
    (tmp_path / 'ui').mkdir()
    (tmp_path / 'catalog').mkdir()
    (tmp_path / 'ui/demo_i18n.c').write_text('"你好"', encoding='utf-8')
    (tmp_path / 'catalog/names.c').write_text('"温度"', encoding='utf-8')
    assert fonts.product_font_text(tmp_path, {}) == '"你好"'
    config = {'text_sources': ['catalog/names.c', 'catalog/names.c']}
    assert fonts.product_font_text(tmp_path, config) == '"你好"\n"温度"'

@pytest.mark.parametrize('paths', ['catalog/names.c', [3], ['../outside.c'], ['/outside.c']])
def test_font_sources_reject_invalid_paths(tmp_path, paths):
    (tmp_path / 'ui').mkdir()
    (tmp_path / 'ui/demo_i18n.c').write_text('', encoding='utf-8')
    with pytest.raises(ValueError):
        fonts.product_font_text(tmp_path, {'text_sources': paths})

def test_font_sources_require_declared_files(tmp_path):
    (tmp_path / 'ui').mkdir()
    (tmp_path / 'ui/demo_i18n.c').write_text('', encoding='utf-8')
    with pytest.raises(FileNotFoundError):
        fonts.product_font_text(tmp_path, {'text_sources': ['missing.c']})
