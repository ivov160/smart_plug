#ifndef __USER_CONFIG_H__
#define __USER_CONFIG_H__

// gdb config
#define GDBSTUB_FREERTOS 1
//#define GDBSTUB_USE_OWN_STACK 1

// 512 bytes for headers and body
#define RECV_BUF_SIZE 512

// 1k bytes for send buffer (json)
#define SEND_BUF_SIZE 1024

// for develop 1 connection
#define CONNECTION_POOL_SIZE 1

#ifdef __GNUC__
#define NORETURN __attribute__((noreturn))
#define NOINLINE __attribute__((noinline))
#define WARN_UNUSED_RESULT __attribute__((warn_unused_result))
#define NOINSTR __attribute__((no_instrument_function))
#else
#define NORETURN
#define NOINLINE
#define WARN_UNUSED_RESULT
#define NOINSTR
#endif /* __GNUC__ */

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(array) (sizeof(array) / sizeof(array[0]))
#endif

#endif
