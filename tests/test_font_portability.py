"""验证字体生成注释不包含构建主机路径。"""
import importlib.util
from pathlib import Path

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
