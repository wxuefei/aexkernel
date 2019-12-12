#pragma once

// AEX return codes
enum aex_return_code {
    ERR_GENERAL       = -0x000001,
    ERR_INV_ARGUMENTS = -0x000002,
    ERR_NO_SPACE      = -0x000003,
    ERR_ALREADY_DONE  = -0x000004,
    ERR_NOT_POSSIBLE  = -0x000005,
    ERR_NOT_FOUND     = -0x000006,
    ERR_TOO_MUCH      = -0x00000F,

    DEV_ERR_NOT_FOUND     = -0x0D0006,
    DEV_ERR_NOT_SUPPORTED = -0x0D000C,

    EXE_ERR_INVALID_CPU  = -0x0E0001,
    EXE_ERR_INVALID_FILE = -0x0E0002,

    FS_ERR_NOT_FOUND  = -0x0F0006,
    FS_ERR_NO_MATCHING_FILESYSTEM = -0x0F0007,
    FS_ERR_IS_DIR     = -0x0F0008,
    FS_ERR_READONLY   = -0x0F0009,
    FS_ERR_NOT_DIR    = -0x0F000A,
    FS_ERR_NOT_OPEN   = -0x0F000B,
    FS_ERR_NOT_A_DEV  = -0x0F000D,
    FS_ERR_WRONG_MODE = -0x0F000E,

    ERR_UNKNOWN       = -0xFFFFFF,
};