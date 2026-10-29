#ifndef HMI_UDS_DID_H
#define HMI_UDS_DID_H
/* UDS DID 契约（线协议唯一定义处，文档必须与本文件一致）。
 *
 * F186/F192/F195 使用 ISO 14229-1 定义的标准系统供应商 DID；
 * 0xFD00-0xFEFF 是标准保留给系统供应商专用的区间，FD10/FD11/FD12 落在此处。
 * 0xF180（bootSoftwareIdentification）已废弃，不得再实现。 */
#define HMI_UDS_DID_UPDATE_MANIFEST 0xfd10u
#define HMI_UDS_DID_UPDATE_STATUS 0xfd11u
#define HMI_UDS_DID_UPDATE_ENDPOINT 0xfd12u
#define HMI_UDS_DID_ACTIVE_SESSION 0xf186u
#define HMI_UDS_DID_HARDWARE_NUMBER 0xf192u
#define HMI_UDS_DID_SOFTWARE_VERSION 0xf195u

/* 定长线协议；字段顺序、长度与字节序由显式编解码函数保证，禁止直接拷贝 C 结构体。 */
#define HMI_UDS_MANIFEST_SIZE 148u
#define HMI_UDS_STATUS_SIZE 12u
#define HMI_UDS_ENDPOINT_SIZE 164u
#define HMI_UDS_ENDPOINT_OS_FILE 16u
#define HMI_UDS_ENDPOINT_REASON 32u
#define HMI_UDS_HARDWARE_SIZE 32u
#define HMI_UDS_SOFTWARE_SIZE 48u
#define HMI_UDS_ID_SIZE 32u

/* FD11 flags 位定义；6-7 保留且固定为 0。 */
#define HMI_UDS_FLAG_BACKEND_SUPPORTED (1u << 0)
#define HMI_UDS_FLAG_MAINTENANCE (1u << 1)
#define HMI_UDS_FLAG_EXCLUSIVE (1u << 2)
#define HMI_UDS_FLAG_ADMISSION (1u << 3)
#define HMI_UDS_FLAG_MANIFEST_PRESENT (1u << 4)
#define HMI_UDS_FLAG_CANDIDATE_VERIFIED (1u << 5)
#endif
