#ifndef _USER_SESSION_H_
#define _USER_SESSION_H_

#include <types.h>

typedef enum {SESSION_NORMAL, SESSION_VM} session_type;

sid_t session(session_type type);
sid_t getsid(void);

#endif /* !_USER_SESSION_H_ */
