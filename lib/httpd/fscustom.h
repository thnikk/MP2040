#ifndef FSCUSTOM_H_
#define FSCUSTOM_H_

#include "fs.h"

#ifdef __cplusplus
extern "C" {
#endif

int fs_open_custom(struct fs_file *file, const char *name);
void fs_close_custom(struct fs_file *file);
#if LWIP_HTTPD_FS_ASYNC_READ
u8_t fs_canread_custom(struct fs_file *file);
u8_t fs_wait_read_custom(struct fs_file *file, fs_wait_cb callback_fn, void *callback_arg);
#endif /* LWIP_HTTPD_FS_ASYNC_READ */

#ifdef __cplusplus
}
#endif

#endif
