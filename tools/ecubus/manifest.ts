/**
 * FD10 Manifest 的编解码：与设备端 encode_manifest()、Host 端 Manifest.encode() 同一线协议。
 *
 * 独立成模块是为了能在没有 EcuBus-Pro 的环境里直接跑（node 可直接执行 .ts），
 * 与 Python 实现逐字节对拍，避免第三份实现悄悄漂移。
 */

export interface DeclaredManifest {
  product: string
  hardware: string
  version: string
  size: number
  sha256: string
}

export const MANIFEST_SIZE = 148

/** 定宽身份字段：ASCII、NUL 终止、剩余补零，规则与设备端 valid_ident 一致。 */
function ident(value: unknown, field: string, width: number): Buffer {
  const text = typeof value === 'string' ? value : ''
  if (!/^[A-Za-z0-9_.+-]+$/.test(text) || text.length >= width) {
    throw new Error(`${field} 必须是 1..${width - 1} 个 [A-Za-z0-9_.+-] 字符，当前为 ${JSON.stringify(value)}`)
  }
  const out = Buffer.alloc(width)
  out.write(text, 'ascii')
  return out
}

export function encodeManifest(declared: DeclaredManifest): Buffer {
  if (!Number.isInteger(declared.size) || declared.size <= 0 || declared.size > 0xffffffff) {
    throw new Error(`size 必须是 1..0xFFFFFFFF 的整数，当前为 ${JSON.stringify(declared.size)}`)
  }
  if (!/^[0-9a-f]{64}$/.test(declared.sha256)) {
    throw new Error('sha256 必须是 64 位小写十六进制')
  }
  const size = Buffer.alloc(4)
  size.writeUInt32BE(declared.size, 0)
  const body = Buffer.concat([
    ident(declared.product, 'product', 32),
    ident(declared.hardware, 'hardware', 32),
    ident(declared.version, 'version', 48),
    size,
    Buffer.from(declared.sha256, 'hex')
  ])
  if (body.length !== MANIFEST_SIZE) {
    throw new Error(`Manifest 组装为 ${body.length} 字节，应为 ${MANIFEST_SIZE}`)
  }
  return body
}

export function decodeManifest(raw: Buffer): DeclaredManifest {
  if (raw.length !== MANIFEST_SIZE) {
    throw new Error(`Manifest 应为 ${MANIFEST_SIZE} 字节，实际 ${raw.length}`)
  }
  const text = (offset: number, width: number) =>
    raw
      .subarray(offset, offset + width)
      .toString('ascii')
      .replace(/\0[\s\S]*$/, '')
  return {
    product: text(0, 32),
    hardware: text(32, 32),
    version: text(64, 48),
    size: raw.readUInt32BE(112),
    sha256: raw.subarray(116, 148).toString('hex')
  }
}
