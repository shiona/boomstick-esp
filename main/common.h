#ifndef _COMMON_H
#define _COMMON_H

#define RETURN_ON_ERR(x) \
{                        \
    int err = x;         \
    if (err) {           \
        return err;      \
    }                    \
}

#endif
