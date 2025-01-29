#include <cstdint>

namespace Protocol {

struct UploadRequestInit {
    enum mode_t : uint32_t {
        NEW         = 0,
        CONTINUE    = 1,
        ANY         = 2
    };

    mode_t mode;
    uint32_t crc32;
    uint64_t size;

    char   name[64];
};

struct UploadRequestTransfer {
    uint64_t start;
    uint32_t resource_id;
    //uint64_t payload_size;        //calculated by common header

    //payload       payload_size - sizeof(UploadRequest) - sizeof(Header)
};

struct UploadRequestCancel {};

union UploadRequestBody {
    UploadRequestTransfer transfer;
    UploadRequestInit init;
    UploadRequestCancel cancel;
};

struct UploadRequest {
    enum code_t : uint32_t {
        INIT        = 0,
        TRANSFER    = 1,
        CANCEL      = 2
    };

    code_t code;
    UploadRequestBody body;
};

struct UploadResponce {
    enum code_t : uint32_t {
        SUCCESS                     = 0,
        CONFIRMED                   = 1,
        ERR_UNRECOGNIZED_REQUEST    = 2,
        ERR_TRANSFER_BEFORE_INIT    = 3,
        ERR_FILE_ALREADY_EXZIST     = 4,
        ERR_FILE_BUF_NOT_EXZIST     = 5,
        ERR_REQUESTED_SIZE_OVERFLOW = 6,
        ERR_INCORRECT_CHUNK_START   = 7,
        ERR_CRC32_MISMATCH          = 8,
        ERR_RESOURCE_ID_MISMATCH    = 9,
        ERR_UNKNOWN                 = 10
    };

    code_t code;

    uint32_t intermediate_crc32;
    uint64_t confirmed_chunk_start;
    uint64_t requested_chunk_start;

    uint64_t full_length;
    uint32_t full_crc32;
    uint32_t resource_id;
};

}

