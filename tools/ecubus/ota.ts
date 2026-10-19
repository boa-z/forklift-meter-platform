/**
 * 叉车仪表 CAN OTA 的 EcuBus-Pro 脚本。
 *
 * 序列本身只能表达定长请求，而 FD10 Manifest 的 size 与 SHA256 必须跟着镜像走。
 * 所以这里只做序列做不到的部分：读 ota.manifest.json、核对镜像、生成 148 字节
 * Manifest 并写进 0x2E 请求；0x34/0x36/0x37 交给内置 RequestDownloadBin 完成。
 */
import { DiagRequest } from 'ECB'
import crypto from 'crypto'
import fs from 'fs/promises'
import path from 'path'
import { MANIFEST_SIZE, encodeManifest, type DeclaredManifest } from './manifest'

const ROOT = process.env.PROJECT_ROOT as string
const DID_MANIFEST = 0xfd10

/** FD12 回读到的设备真实身份，用于在 0x34 之前给出可读的拒绝原因。 */
let deviceProduct = ''
let deviceHardware = ''

function fail(message: string): never {
  console.error(`[OTA] ${message}`)
  throw new Error(message)
}

async function readJson<T>(file: string): Promise<T> {
  let text: string
  try {
    text = await fs.readFile(file, 'utf8')
  } catch (error) {
    fail(`读不到 ${file}：${(error as Error).message}`)
  }
  try {
    return JSON.parse(text) as T
  } catch (error) {
    fail(`${file} 不是合法 JSON：${(error as Error).message}`)
  }
}

Util.Init(() => {
  console.log(`[OTA] 脚本已加载，工程根目录 ${ROOT}`)
})

/** 0x22 FD12 的应答：拿到设备身份，和升级包比对，不一致就不必再走 0x34。 */
Util.On('Meter.ReadEndpoint.recv', (resp) => {
  const data = resp.diagGetParameterRaw('endpoint')
  deviceProduct = data
    .subarray(0, 32)
    .toString('ascii')
    .replace(/\0[\s\S]*$/, '')
  deviceHardware = data
    .subarray(32, 64)
    .toString('ascii')
    .replace(/\0[\s\S]*$/, '')
  console.log(`[OTA] 设备身份 product=${deviceProduct} hardware=${deviceHardware}`)
})

/**
 * 序列步骤：在 0x2E 之前把 Manifest 准备好。
 * 返回空数组，表示本步骤不追加任何请求。
 */
Util.Register('Meter.PrepareManifest', async (): Promise<DiagRequest[]> => {
  const config = await readJson<{ firmware: string; manifest: string }>(path.join(ROOT, 'ota.config.json'))
  const imagePath = path.resolve(ROOT, config.firmware)
  const manifestPath = path.resolve(ROOT, config.manifest)
  const declared = await readJson<DeclaredManifest>(manifestPath)

  let image: Buffer
  try {
    image = await fs.readFile(imagePath)
  } catch (error) {
    fail(`读不到镜像 ${imagePath}：${(error as Error).message}`)
  }
  /* Manifest 是升级包的声明，镜像才是真正要写进 Flash 的字节；两者必须同一个文件。 */
  if (image.length !== declared.size) {
    fail(`镜像长度 ${image.length} 与 Manifest 声明的 ${declared.size} 不一致`)
  }
  const digest = crypto.createHash('sha256').update(image).digest('hex')
  if (digest !== declared.sha256) {
    fail(`镜像 SHA256 ${digest} 与 Manifest 声明的 ${declared.sha256} 不一致`)
  }
  if (deviceProduct && deviceProduct !== declared.product) {
    fail(`设备 Product 为 ${deviceProduct}，升级包为 ${declared.product}`)
  }
  if (deviceHardware && deviceHardware !== declared.hardware) {
    fail(`设备 Hardware 为 ${deviceHardware}，升级包为 ${declared.hardware}`)
  }

  let body: Buffer
  try {
    body = encodeManifest(declared)
  } catch (error) {
    fail((error as Error).message)
  }

  const request = DiagRequest.from('Meter.WriteManifest')
  request.diagSetParameter('dataIdentifier', DID_MANIFEST)
  request.diagSetParameterSize('manifest', MANIFEST_SIZE * 8)
  request.diagSetParameterRaw('manifest', body)
  await request.changeService()

  console.log(
    `[OTA] Manifest 就绪：${declared.product}/${declared.hardware}/${declared.version} ` +
      `${declared.size} 字节 sha256=${declared.sha256.slice(0, 16)}…`
  )
  return []
})
