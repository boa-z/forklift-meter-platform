#ifndef TEST_FINSH_H
#define TEST_FINSH_H
/* 复用生产 MSH 函数体，仅替换宿主无法链接的原生注册段。 */
#define MSH_CMD_EXPORT(fn, description)                                                                      \
    int test_msh_##fn(int argc, char **argv)                                                                 \
    {                                                                                                        \
        return fn(argc, argv);                                                                               \
    }
#endif
