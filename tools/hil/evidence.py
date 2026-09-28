"""每次运行保存独立证据；物理结果和 Host 结果严格分开。"""
from datetime import datetime, timezone
import json
from pathlib import Path
import uuid


class Evidence:
    """创建唯一目录并即时保存 JSON/文本证据。"""
    def __init__(self, root):
        self.path = Path(root) / (datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ')+'-'+uuid.uuid4().hex[:8])
        self.path.mkdir(parents=True, exist_ok=False)
        self.metadata = dict(status='HIL_NOT_RUN', reference_mixed='NOT_RUN', tests=[])
        self.save()

    def text(self, name, data):
        """保存完整命令文本，不用解析值替代原始证据。"""
        (self.path/name).write_text(data, encoding='utf-8')

    def save(self):
        """原子替换元数据，避免中断留下截断的 JSON。"""
        temporary = self.path/'metadata.tmp'
        temporary.write_text(json.dumps(self.metadata, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
        temporary.replace(self.path/'metadata.json')

    def capture(self, dut, prefix='failure'):
        """失败时每项独立采集；一个命令失败不会阻断其余证据。"""
        errors = {}
        for name, command in [('diag', 'meter diag'), ('runtime', 'meter runtime'), ('can', 'meter can'), ('domain', 'meter domain'), ('trace', 'meter trace dump')]:
            try:
                self.text(prefix+'-'+name+'.txt', dut.command(command, timeout=10 if name == 'trace' else 5))
            except Exception as exc:
                errors[command] = str(exc)
        if errors:
            self.text(prefix+'-capture-errors.json', json.dumps(errors, indent=2))
        return errors
