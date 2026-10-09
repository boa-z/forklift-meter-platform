#!/usr/bin/env python3
"""生成独立产品最小模板；只使用公开模板，不复制 Demo 业务或视觉。"""
import argparse
from pathlib import Path
import re
import subprocess
import sys
ROOT = Path(__file__).resolve().parents[1]

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--id',required=True)
    p.add_argument('--output',required=True,type=Path)
    a=p.parse_args()
    if not re.fullmatch(r'[a-z][a-z0-9]*(?:-[a-z0-9]+)*',a.id): p.error('Use a lowercase hyphenated product ID')
    if a.output.exists(): p.error('Output already exists; no files were modified')
    template=ROOT/'tools/product_template'
    for source in template.rglob('*'):
        if not source.is_file(): continue
        target=a.output/source.relative_to(template)
        target.parent.mkdir(parents=True,exist_ok=True)
        target.write_text(source.read_text(encoding='utf-8').replace('@PRODUCT_ID@',a.id),encoding='utf-8',newline='\n')
    subprocess.run([sys.executable,str(ROOT/'tools/protocol/generate_can.py'),'--product-root',str(a.output.resolve())],check=True)
    print(f'Created {a.output}; select with -DMETER_PRODUCT_ROOT={a.output.resolve()}')
if __name__=='__main__': main()
