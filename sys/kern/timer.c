#include <sys/debug.h>
#include <sys/pcpu.h>
#include <sys/spinlock.h>
#include <sys/string.h>
#include <sys/timer.h>
#include <sys/types.h>
#include <sys/x86.h>

#define MAX_TIMER	256

struct timer {
	/* should be acquired before every access */
	spinlock_t	lk;

	/* is this timer is enabled? */
	bool		enabled;

	/* when the timerout will happen (in TSC ticks) */
	uint64_t	expire_time;
	/*
	 * When count decreases to 0, timeout_handler() will be invoked to
	 * handle this timeout event.
	 */
	void		(*timeout_handler)(void*, uint64_t);
	/* parameters required by timeout_handler() */
	void		*params;
	/* is this timer being used? */
	bool		used;
};

/*
 * XXX: Should the timer list be global or be local to processor core?
 * FIXME: In the later case, move the list to pcpu structure.
 */
static struct timer	timer_list[MAX_TIMER];
static spinlock_t 	timer_list_lk;
static volatile bool	timer_list_inited = FALSE;

void
timer_init(void)
{
	int i;

	KERN_ASSERT(MAX_TIMER < INVALID_TIMER);

	if (pcpu_onboot() == FALSE) {
		KERN_WARN("Only BSP is allowed to initialize timers.\n");
		return;
	}

	/* timers can only be initialized once */
	KERN_ASSERT(timer_list_inited == FALSE);

	spinlock_init(&timer_list_lk);

	spinlock_acquire(&timer_list_lk);

	memset(timer_list, 0x0, sizeof(struct timer) * MAX_TIMER);
	for (i = 0; i < MAX_TIMER; i++) {
		spinlock_init(&timer_list[i].lk);
		timer_list[i].used = FALSE;
	}

	spinlock_release(&timer_list_lk);

	timer_list_inited = TRUE;
}

/*
 * Add a timer to the global timer list.
 *
 * @param count the initial count of the timer
 * @param handler the function to be invoked when timer countdowns to zero
 * @param params the parameters required by handler
 *
 * @return if succeeded, return the timer id; otherwise, INVALID_TIMER
 */
timer_t
timer_add(uint64_t expire_time, void (*handler)(void *, uint64_t), void *params)
{
	int i;
	struct timer *iter, *free_timer;

	/* search for free timer slot */
	free_timer = NULL;
	spinlock_acquire(&timer_list_lk);
	for (i = 0; i < MAX_TIMER && free_timer == NULL; i++) {
		iter = &timer_list[i];

		spinlock_acquire(&iter->lk);
		/*
		 * If a free timer slot is found, keep it locked and exit the
		 * loop, so that we can safely release timer_list_lock and keep
		 * the free timer slot inaccessible by other subjects meanwhile.
		 */
		if (iter->used == FALSE)
			free_timer = iter;
		else
			spinlock_release(&iter->lk);
	}
	spinlock_release(&timer_list_lk);

	if (free_timer == NULL) {
		KERN_DEBUG("Failed to find free timer.\n");
		return INVALID_TIMER;
	}

	/* fill up the free timer slot */
	KERN_ASSERT(spinlock_holding(&free_timer->lk) == TRUE);

	free_timer->expire_time = expire_time;
	free_timer->timeout_handler = handler;
	free_timer->params = params;
	free_timer->used = TRUE;

	spinlock_release(&free_timer->lk);

	KERN_DEBUG("Add timer %x: %llx:%llx.\n", i, expire_time, rdtsc());

	return (timer_t) (i-1);
}

/*
 * Remove a timer from the global timer list.
 * XXX: the remove operation will clear the count, timeout_handler, and params
 *      fields of the timer.
 *
 * @param timer the timer to be removed
 *
 * @return 0 if no errors happen
 */
int
timer_remove(timer_t timer_id)
{
	int ret;
	struct timer *timer;

	if (timer_id >= MAX_TIMER)
		return 1;

	spinlock_acquire(&timer_list_lk);
	timer = &timer_list[timer_id];
	spinlock_release(&timer_list_lk);

	spinlock_acquire(&timer->lk);

	if (timer->used == FALSE) {
		ret = 1;
		goto out;
	}

	timer->expire_time = 0;
	timer->timeout_handler = NULL;
	timer->params = NULL;
	timer->used = FALSE;
	ret = 0;

	KERN_DEBUG("Remove timer %x.\n", timer_id);

 out:
	spinlock_release(&timer->lk);
	return ret;
}

/*
 * Update the expiring time of a timer in the gloabl timer list.
 *
 * @param timer_id the id of the timer
 * @param expire_time the new expiring time
 *
 * @return 0 if no errors happen
 */
int
timer_update_expire_time(timer_t timer_id, uint64_t expire_time)
{
	struct timer *timer;
	int ret;

	if (timer_id >= MAX_TIMER)
		return 1;

	spinlock_acquire(&timer_list_lk);
	timer = &timer_list[timer_id];
	spinlock_release(&timer_list_lk);

	spinlock_acquire(&timer->lk);

	if (timer->used == FALSE) {
		ret = 1;
		goto out;
	}

	timer->expire_time = expire_time;
	ret = 0;

	KERN_DEBUG("Update timer %x: %llx:%llx.\n",
		   timer_id, expire_time, rdtsc());

 out:
	spinlock_release(&timer->lk);
	return ret;
}

int
timer_enable(timer_t timer_id)
{
	struct timer *timer;
	int ret;

	if (timer_id >= MAX_TIMER)
		return 1;

	spinlock_acquire(&timer_list_lk);
	timer = &timer_list[timer_id];
	spinlock_release(&timer_list_lk);

	spinlock_acquire(&timer->lk);

	if (timer->used == FALSE) {
		ret = 1;
		goto out;
	}

	timer->enabled = TRUE;
	ret = 0;

	KERN_DEBUG("Enable timer %x: %llx:%llx.\n",
		   timer_id, timer->expire_time, rdtsc());

 out:
	spinlock_release(&timer->lk);
	return ret;
}

int
timer_disable(timer_t timer_id)
{
	struct timer *timer;
	int ret;

	if (timer_id >= MAX_TIMER)
		return 1;

	spinlock_acquire(&timer_list_lk);
	timer = &timer_list[timer_id];
	spinlock_release(&timer_list_lk);

	spinlock_acquire(&timer->lk);

	if (timer->used == FALSE) {
		ret = 1;
		goto out;
	}

	timer->enabled = FALSE;
	ret = 0;

	KERN_DEBUG("Disable timer %x: :%llx.\n",
		   timer_id, rdtsc());

 out:
	spinlock_release(&timer->lk);
	return ret;
}

bool
timer_is_enabled(timer_t timer_id)
{
	struct timer *timer;
	bool ret;

	if (timer_id >= MAX_TIMER)
		return FALSE;

	spinlock_acquire(&timer_list_lk);
	timer = &timer_list[timer_id];
	spinlock_release(&timer_list_lk);

	spinlock_acquire(&timer->lk);
	ret = timer->enabled;
	spinlock_release(&timer->lk);

	return ret;
}

void
timer_handle_timeout(void)
{
	int i;
	uint64_t current_time = rdtsc();
	struct timer *timer;
	void (*timeout_handler)(void *, uint64_t);
	void *params;

	spinlock_acquire(&timer_list_lk);

	for (i = 0; i < MAX_TIMER; i++) {
		timer = &timer_list[i];

		spinlock_acquire(&timer->lk);

		if (timer->used == FALSE || timer->enabled == FALSE ||
		    timer->expire_time > current_time) {
			spinlock_release(&timer->lk);
			continue;
		}

		KERN_DEBUG("Trigger timer %d: %llx.\n", i, timer->expire_time);

		timer->enabled = FALSE;
		timeout_handler = timer->timeout_handler;
		params = timer->params;

		spinlock_release(&timer->lk);

		if (timeout_handler != NULL) {
			spinlock_release(&timer_list_lk);
			timeout_handler(params, current_time);
			spinlock_acquire(&timer_list_lk);
		}
	}

	spinlock_release(&timer_list_lk);
}
