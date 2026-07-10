#pragma once

#define E_OK        0
#define E_NOENT    -1  /* file/entry not found */
#define E_BADF     -2  /* bad file descriptor */
#define E_FAULT    -3  /* bad user pointer */
#define E_NOMEM    -4  /* OOM */
#define E_PERM     -5  /* permission denied (e.g. RAMDISK write) */
#define E_AGAIN    -6  /* no proc slots / would block */
