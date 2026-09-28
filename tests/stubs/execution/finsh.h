#pragma once
/* Preserve the native registration reference without providing a shell.
 * GNU/Clang fixture targets use the same retained-export convention. */
#define MSH_CMD_EXPORT(fn, description) \
    static int (*const test_shell_export_##fn)(int, char **) __attribute__((used)) = fn
