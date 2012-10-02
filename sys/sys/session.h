#ifndef _KERN_SESSION_H_
#define _KERN_SESSION_H_

#ifdef _KERN_

#include <sys/gcc.h>
#include <sys/mmu.h>
#include <sys/proc.h>
#include <sys/queue.h>
#include <sys/spinlock.h>
#include <sys/types.h>

#include <sys/virt/vmm.h>

#define MAX_SID		128

struct session {
	sid_t sid;

	enum { SESSION_NORMAL, SESSION_VM } type;
	struct vm *vm;
	LIST_HEAD(session_proc_list, proc) proc_list;

	spinlock_t lk;
};

/*
 * Initialize the session module.
 */
void session_init(void);

/*
 * Create a new process session.
 *
 * @param type if type is SESSION_NORMAL, a normal session will be created;
 *             if type is SESSION_VM, a virtual machine will be created
 *
 * @return the pointer to the session structure if successful;
 *         otherwise, return NULL.
 */
struct session *session_new(int type);

/*
 * Free a process session. The operation succeeds only if there is no processes
 * and virtual machines in the session.
 *
 * @param s the session to be freed
 *
 * @return 0 if successful; otherwise, return a non-zero value.
 */
int session_free(struct session *s);

/*
 * Add a process to the process session.
 *
 * @param s which process session the process will be added to
 * @param p which process will be added
 *
 * @return 0 if successful; otherwise, return a non-zero value.
 */
int session_add_proc(struct session *s, struct proc *p);

/*
 * Remove a process from the process session.
 *
 * @param s which process session the process belongs to
 * @param p which process will be removed
 *
 * @return 0 if successful; otherwise, return a non-zero value.
 */
int session_remove_proc(struct session *s, struct proc *p);

/*
 * Add a virtual machine to the process session. Only a virtual machine session
 * can host at most one virtual machine.
 *
 * @param s  which process session the virtual machine will be added to
 * @param vm which virtual machine will be added
 *
 * @return 0 if successful; otherwise, return a non-zero value.
 */
int session_add_vm(struct session *s, struct vm *vm);

/*
 * Remove the virtual machine from the process session.
 *
 * @param s which process session hosts the virtual machine
 *
 * @return 0 if successful; otherwise, return a non-zero value.
 */
int session_remove_vm(struct session *s);

/*
 * Get the session structure of session sid.
 *
 * @param sid the session id
 *
 * @return the session structure if the session id is valid; otherwise, return
 *         NULL.
 */
struct session *session_get_session(sid_t sid);

#endif /* _KERN_ */

#endif /* !_KERN_SESSION_H_ */
