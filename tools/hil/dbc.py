"""正常激励只通过 cantools 编码；边界向量来自 DBC 元数据。"""
from decimal import Decimal, ROUND_CEILING, ROUND_FLOOR, ROUND_HALF_UP
import hashlib
from pathlib import Path
import can
import cantools


class Dbc:
    """提供唯一 wire source 以及可保存的确定性边界数据。"""
    def __init__(self, path):
        self.path = Path(path)
        self.sha256 = hashlib.sha256(self.path.read_bytes()).hexdigest()
        self.database = cantools.database.load_file(str(path))

    def encode(self, name, signals):
        """严格编码正常帧，拒绝缺失信号和越界值。"""
        definition = self.database.get_message_by_name(name)
        data = self.database.encode_message(name, signals, strict=True)
        return can.Message(arbitration_id=definition.frame_id, is_extended_id=definition.is_extended_frame,
                           data=data, check=True)

    def vectors(self, message, signal, nominal=None):
        """生成可表示的边界/过零/枚举值；不使用随机数据。"""
        sig = self.database.get_message_by_name(message).get_signal_by_name(signal)
        if sig.is_float:
            raise ValueError('IEEE floating point DBC signals need an explicit vector policy')
        scale, offset = Decimal(str(sig.scale)), Decimal(str(sig.offset))
        if not scale:
            raise ValueError('zero DBC scale')
        raw_min = -(1 << (sig.length-1)) if sig.is_signed else 0
        raw_max = (1 << (sig.length-1))-1 if sig.is_signed else (1 << sig.length)-1
        physical = sorted([Decimal(raw_min)*scale+offset, Decimal(raw_max)*scale+offset])
        low = Decimal(str(sig.minimum)) if sig.minimum is not None else physical[0]
        high = Decimal(str(sig.maximum)) if sig.maximum is not None else physical[1]
        bounds = sorted([(low-offset)/scale, (high-offset)/scale])
        lo = max(raw_min, int(bounds[0].to_integral_value(rounding=ROUND_CEILING)))
        hi = min(raw_max, int(bounds[1].to_integral_value(rounding=ROUND_FLOOR)))
        if lo > hi: raise ValueError('DBC range has no representable value')
        if sig.choices:
            pairs = [('enum:'+str(v), int(k)) for k, v in sorted(sig.choices.items())]
        else:
            first, last, step = (lo, hi, 1) if scale > 0 else (hi, lo, -1)
            center = Decimal(str(nominal)) if nominal is not None else (low+high)/2
            middle = min(hi, max(lo, int(((center-offset)/scale).to_integral_value(rounding=ROUND_HALF_UP))))
            pairs = [('min', first), ('min+1LSB', first+step), ('nominal', middle), ('max-1LSB', last-step), ('max', last)]
            if low < 0 < high:
                for name, v in [('-1LSB', -abs(scale)), ('zero', Decimal(0)), ('+1LSB', abs(scale))]:
                    raw = (v-offset)/scale
                    if raw == raw.to_integral_value(): pairs.append((name, int(raw)))
        result = []
        for label, raw in pairs:
            if lo <= raw <= hi:
                result.append(dict(message=message, signal=signal, label=label, raw=raw, value=float(Decimal(raw)*scale+offset)))
        return result


def main():
    """打印或保存确定性向量，不打开 CAN 或 UART。"""
    import argparse
    import json
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('dbc', type=Path)
    parser.add_argument('message')
    parser.add_argument('signal')
    parser.add_argument('--nominal', type=float)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    source = Dbc(args.dbc)
    result = dict(dbc_sha256=source.sha256, vectors=source.vectors(args.message, args.signal, args.nominal))
    text = json.dumps(result, ensure_ascii=False, indent=2)+'\n'
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(text, encoding='utf-8')
    else:
        print(text, end='')


if __name__ == '__main__':
    main()
