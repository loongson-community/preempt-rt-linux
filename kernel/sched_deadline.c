/*
 * Deadline Scheduling Class (SCHED_DEADLINE policy)
 *
 * This scheduling class implements the Earliest Deadline First (EDF)
 * scheduling algorithm, suited for hard and soft real-time tasks.
 *
 * The strategy used to confine each task inside its bandwidth reservation
 * is the Constant Bandwidth Server (CBS) scheduling, a slight variation on
 * EDF that makes this possible.
 *
 * Correct behavior, i.e., no task missing any deadline, is only guaranteed
 * if the task's parameters are:
 *  - correctly assigned, so that the system is not overloaded,
 *  - respected during actual execution.
 * However, thanks to bandwidth isolation, overruns and deadline misses
 * remains local, and does not affect any other task in the system.
 *
 * Groups, if configured, have bandwidth as well, and it is enforced that
 * the sum of the bandwidths of entities (tasks and groups) belonging to
 * a group stays below its own bandwidth.
 *
 * Copyright (C) 2009 Dario Faggioli, Michael Trimarchi
 */

static const struct sched_class deadline_sched_class;

static inline struct task_struct *deadline_task_of(struct sched_dl_entity *dl_se)
{
	return container_of(dl_se, struct task_struct, dl);
}

static inline struct rq *rq_of_deadline_rq(struct dl_rq *dl_rq)
{
	return container_of(dl_rq, struct rq, dl);
}

static inline struct dl_rq *deadline_rq_of_se(struct sched_dl_entity *dl_se)
{
	struct task_struct *p = deadline_task_of(dl_se);
	struct rq *rq = task_rq(p);

	return &rq->dl;
}

/*
 * FIXME:
 *  This is broken for now, correct implementation of a BWI/PEP
 *  solution is needed here!
 */
static inline int deadline_se_boosted(struct sched_dl_entity *dl_se)
{
	struct task_struct *p = deadline_task_of(dl_se);

	return p->prio != p->normal_prio;
}

static inline int on_deadline_rq(struct sched_dl_entity *dl_se)
{
	return !RB_EMPTY_NODE(&dl_se->rb_node);
}

#define for_each_leaf_deadline_rq(dl_rq, rq) \
	for (dl_rq = &rq->dl; dl_rq; dl_rq = NULL)

static inline int deadline_time_before(u64 a, u64 b)
{
	return (s64)(a - b) < 0;
}

static inline u64 deadline_max_deadline(u64 a, u64 b)
{
	s64 delta = (s64)(b - a);
	if (delta > 0)
		a = b;

	return a;
}

static void enqueue_deadline_entity(struct sched_dl_entity *dl_se);
static void dequeue_deadline_entity(struct sched_dl_entity *dl_se);
static void check_deadline_preempt_curr(struct task_struct *p, struct rq *rq);

/*
 * setup a new SCHED_DEADLINE task instance.
 */
static inline void setup_new_deadline_entity(struct sched_dl_entity *dl_se)
{
	struct dl_rq *dl_rq = deadline_rq_of_se(dl_se);
	struct rq *rq = rq_of_deadline_rq(dl_rq);

	dl_se->flags &= ~DL_NEW;
	dl_se->deadline = max(dl_se->deadline, rq->clock) +
			      dl_se->sched_deadline;
	dl_se->runtime = dl_se->sched_runtime;
}

/*
 * gives a SCHED_DEADLINE task that run out of runtime the possibility
 * of restarting executing, with a refilled runtime and a new
 * (postponed) deadline.
 */
static void replenish_deadline_entity(struct sched_dl_entity *dl_se)
{
	struct dl_rq *dl_rq = deadline_rq_of_se(dl_se);
	struct rq *rq = rq_of_deadline_rq(dl_rq);

	/*
	 * Keep moving the deadline and replenishing runtime by the
	 * proper amount until the runtime becomes positive.
	 */
	while (dl_se->runtime < 0) {
		dl_se->deadline += dl_se->sched_deadline;
		dl_se->runtime += dl_se->sched_runtime;
	}

	WARN_ON(dl_se->runtime > dl_se->sched_runtime);
	WARN_ON(deadline_time_before(dl_se->deadline, rq->clock));
}

static void update_deadline_entity(struct sched_dl_entity *dl_se)
{
	struct dl_rq *dl_rq = deadline_rq_of_se(dl_se);
	struct rq *rq = rq_of_deadline_rq(dl_rq);
	u64 left, right;

	if (dl_se->flags & DL_NEW) {
		setup_new_deadline_entity(dl_se);
		return;
	}

	/*
	 * Update the deadline of the task only if:
	 * - the budget has been completely exhausted;
	 * - using the ramaining budget, with the current deadline, would
	 *   make the task exceed its bandwidth;
	 * - the deadline itself is in the past.
	 *
	 * For the second condition to hold, we check if:
	 *  runtime / (deadline - rq->clock) >= sched_runtime / sched_deadline
	 *
	 * Which basically says if, in the time left before the current
	 * deadline, the tasks overcome its expected runtime by using the
	 * residual budget (left and right are the two sides of the equation,
	 * after a bit of shuffling to use multiplications instead of
	 * divisions).
	 */
	if (deadline_time_before(dl_se->deadline, rq->clock))
		goto update;

	left = dl_se->sched_deadline * dl_se->runtime;
	right = (dl_se->deadline - rq->clock) * dl_se->sched_runtime;

	if (deadline_time_before(right, left)) {
update:
		dl_se->deadline = rq->clock + dl_se->sched_deadline;
		dl_se->runtime = dl_se->sched_runtime;
	}
}

/*
 * the task just depleted its runtime, so we try to post the
 * replenishment timer to fire at the next absolute deadline.
 *
 * In fact, the task was allowed to execute for at most sched_runtime
 * over each period of sched_deadline length.
 */
static int start_deadline_timer(struct sched_dl_entity *dl_se, u64 wakeup)
{
	struct dl_rq *dl_rq = deadline_rq_of_se(dl_se);
	struct rq *rq = rq_of_deadline_rq(dl_rq);
	ktime_t now, act;
	ktime_t soft, hard;
	unsigned long range;
	s64 delta;

	act = ns_to_ktime(wakeup);
	now = hrtimer_cb_get_time(&dl_se->dl_timer);
	delta = ktime_to_ns(now) - rq->clock;
	act = ktime_add_ns(act, delta);

	hrtimer_set_expires(&dl_se->dl_timer, act);

	soft = hrtimer_get_softexpires(&dl_se->dl_timer);
	hard = hrtimer_get_expires(&dl_se->dl_timer);
	range = ktime_to_ns(ktime_sub(hard, soft));
	__hrtimer_start_range_ns(&dl_se->dl_timer, soft,
				 range, HRTIMER_MODE_ABS, 0);

	return hrtimer_active(&dl_se->dl_timer);
}

static enum hrtimer_restart deadline_timer(struct hrtimer *timer)
	__acquires(rq->lock)
{
	struct sched_dl_entity *dl_se = container_of(timer,
						     struct sched_dl_entity,
						     dl_timer);
	struct task_struct *p = deadline_task_of(dl_se);
	struct dl_rq *dl_rq = deadline_rq_of_se(dl_se);
	struct rq *rq = rq_of_deadline_rq(dl_rq);

	atomic_spin_lock(&rq->lock);

	/*
	 * the task might have changed scheduling policy
	 * through setscheduler_ex, in what case we just do nothing.
	 */
	if (!deadline_task(p))
		goto unlock;

	/*
	 * the task can't be enqueued any the SCHED_DEADLINE runqueue,
	 * and needs to be enqueued back there --with its new deadline--
	 * only if it is active.
	 */
	dl_se->flags &= ~DL_THROTTLED;
	if (p->se.on_rq) {
		replenish_deadline_entity(dl_se);
		enqueue_deadline_entity(dl_se);
		check_deadline_preempt_curr(p, rq);
	}
unlock:
	atomic_spin_unlock(&rq->lock);

	return HRTIMER_NORESTART;
}

static void init_deadline_timer(struct hrtimer *timer)
{
	if (hrtimer_active(timer)) {
		hrtimer_try_to_cancel(timer);
		return;
	}

	hrtimer_init(timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	timer->function = deadline_timer;
}

static void init_deadline_task(struct task_struct *p)
{
	RB_CLEAR_NODE(&p->dl.rb_node);
	init_deadline_timer(&p->dl.dl_timer);
	p->dl.sched_runtime = p->dl.runtime = 0;
	p->dl.sched_deadline = p->dl.deadline = 0;
	p->dl.flags = p->dl.bw = 0;
}

static
int deadline_runtime_exceeded(struct rq *rq, struct sched_dl_entity *dl_se)
{
	/*
	 * if the user asked for that, we have to inform him about
	 * a (scheduling) deadline miss ...
	 */
	if (unlikely(dl_se->flags & SCHED_SIG_DMISS &&
	    deadline_time_before(dl_se->deadline, rq->clock)))
		dl_se->flags |= DL_DMISS;

	if (dl_se->runtime >= 0 || deadline_se_boosted(dl_se))
		return 0;

	/*
	 * ... and the same appies to runtime overruns.
	 *
	 * Note that (hopefully small) runtime overruns are very likely
	 * to occur, mainly due to accounting resolution, while missing a
	 * scheduling deadline should happen only on oversubscribed systems.
	 */
	if (dl_se->flags & SCHED_SIG_RORUN)
		dl_se->flags |= DL_RORUN;

	dequeue_deadline_entity(dl_se);
	if (!start_deadline_timer(dl_se, dl_se->deadline)) {
		replenish_deadline_entity(dl_se);
		enqueue_deadline_entity(dl_se);
	} else
		dl_se->flags |= DL_THROTTLED;

	return 1;
}

static void update_curr_deadline(struct rq *rq)
{
	struct task_struct *curr = rq->curr;
	struct sched_dl_entity *dl_se = &curr->dl;
	u64 delta_exec;

	if (!deadline_task(curr) || !on_deadline_rq(dl_se))
		return;

	delta_exec = rq->clock - curr->se.exec_start;
	if (unlikely((s64)delta_exec < 0))
		delta_exec = 0;

	schedstat_set(curr->se.exec_max, max(curr->se.exec_max, delta_exec));

	curr->se.sum_exec_runtime += delta_exec;
	account_group_exec_runtime(curr, delta_exec);

	curr->se.exec_start = rq->clock;
	cpuacct_charge(curr, delta_exec);

	dl_se->runtime -= delta_exec;
	if (deadline_runtime_exceeded(rq, dl_se))
		resched_task(curr);
}

static void enqueue_deadline_entity(struct sched_dl_entity *dl_se)
{
	struct dl_rq *dl_rq = deadline_rq_of_se(dl_se);
	struct rb_node **link = &dl_rq->rb_root.rb_node;
	struct rb_node *parent = NULL;
	struct sched_dl_entity *entry;
	int leftmost = 1;

	BUG_ON(!RB_EMPTY_NODE(&dl_se->rb_node));

	while (*link) {
		parent = *link;
		entry = rb_entry(parent, struct sched_dl_entity, rb_node);
		if (!deadline_time_before(entry->deadline, dl_se->deadline))
			link = &parent->rb_left;
		else {
			link = &parent->rb_right;
			leftmost = 0;
		}
	}

	if (leftmost)
		dl_rq->rb_leftmost = &dl_se->rb_node;

	rb_link_node(&dl_se->rb_node, parent, link);
	rb_insert_color(&dl_se->rb_node, &dl_rq->rb_root);

	dl_rq->dl_nr_running++;
}

static void dequeue_deadline_entity(struct sched_dl_entity *dl_se)
{
	struct dl_rq *dl_rq = deadline_rq_of_se(dl_se);

	if (RB_EMPTY_NODE(&dl_se->rb_node))
		return;

	if (dl_rq->rb_leftmost == &dl_se->rb_node) {
		struct rb_node *next_node;
		struct sched_dl_entity *next;

		next_node = rb_next(&dl_se->rb_node);
		dl_rq->rb_leftmost = next_node;

		if (next_node)
			next = rb_entry(next_node, struct sched_dl_entity,
					rb_node);
	}

	rb_erase(&dl_se->rb_node, &dl_rq->rb_root);
	RB_CLEAR_NODE(&dl_se->rb_node);

	dl_rq->dl_nr_running--;
}

static void check_preempt_curr_deadline(struct rq *rq, struct task_struct *p,
				   int sync)
{
	if (deadline_task(p) &&
	    deadline_time_before(p->dl.deadline, rq->curr->dl.deadline))
		resched_task(rq->curr);
}

/*
 * there are a few cases where is important to check if a SCHED_DEADLINE
 * task p should preempt the current task of a runqueue (e.g., inside the
 * replenishment timer code).
 */
static void check_deadline_preempt_curr(struct task_struct *p, struct rq *rq)
{
	if (!deadline_task(rq->curr) ||
	    deadline_time_before(p->dl.deadline, rq->curr->dl.deadline))
		resched_task(rq->curr);
}

static void
enqueue_task_deadline(struct rq *rq, struct task_struct *p, int wakeup)
{
	struct sched_dl_entity *dl_se = &p->dl;

	BUG_ON(on_deadline_rq(dl_se));

	/*
	 * Only enqueue entities with some remaining runtime.
	 */
	if (dl_se->flags & DL_THROTTLED)
		return;

	update_deadline_entity(dl_se);
	enqueue_deadline_entity(dl_se);
}

static void
dequeue_task_deadline(struct rq *rq, struct task_struct *p, int sleep)
{
	struct sched_dl_entity *dl_se = &p->dl;

	if (!on_deadline_rq(dl_se))
		return;

	update_curr_deadline(rq);
	dequeue_deadline_entity(dl_se);
}

static void yield_task_deadline(struct rq *rq)
{
}

/*
 * Informs the scheduler that an instance ended.
 */
static void wait_interval_deadline(struct task_struct *p)
{
	p->dl.flags |= DL_NEW;
}

#ifdef CONFIG_SCHED_HRTICK
static void start_hrtick_deadline(struct rq *rq, struct task_struct *p)
{
	struct sched_dl_entity *dl_se = &p->dl;
	s64 delta;

	delta = dl_se->sched_runtime - dl_se->runtime;

	if (delta > 10000)
		hrtick_start(rq, delta);
}
#else
static void start_hrtick_deadline(struct rq *rq, struct task_struct *p)
{
}
#endif

static struct sched_dl_entity *__pick_deadline_last_entity(struct dl_rq *dl_rq)
{
	struct rb_node *last = rb_last(&dl_rq->rb_root);

	if (!last)
		return NULL;

	return rb_entry(last, struct sched_dl_entity, rb_node);
}

static struct sched_dl_entity *pick_next_deadline_entity(struct rq *rq,
							 struct dl_rq *dl_rq)
{
	struct rb_node *left = dl_rq->rb_leftmost;

	if (!left)
		return NULL;

	return rb_entry(left, struct sched_dl_entity, rb_node);
}

struct task_struct *pick_next_task_deadline(struct rq *rq)
{
	struct sched_dl_entity *dl_se;
	struct task_struct *p;
	struct dl_rq *dl_rq;

	dl_rq = &rq->dl;

	if (likely(!dl_rq->dl_nr_running))
		return NULL;

	dl_se = pick_next_deadline_entity(rq, dl_rq);
	BUG_ON(!dl_se);

	p = deadline_task_of(dl_se);
	p->se.exec_start = rq->clock;
#ifdef CONFIG_SCHED_HRTICK
	if (hrtick_enabled(rq))
		start_hrtick_deadline(rq, p);
#endif
	return p;
}

static void put_prev_task_deadline(struct rq *rq, struct task_struct *p)
{
	update_curr_deadline(rq);
	p->se.exec_start = 0;
}

static void task_tick_deadline(struct rq *rq, struct task_struct *p, int queued)
{
	update_curr_deadline(rq);

#ifdef CONFIG_SCHED_HRTICK
	if (hrtick_enabled(rq) && queued && p->dl.runtime > 0)
		start_hrtick_deadline(rq, p);
#endif
}

static void set_curr_task_deadline(struct rq *rq)
{
	struct task_struct *p = rq->curr;

	p->se.exec_start = rq->clock;
}

static void prio_changed_deadline(struct rq *rq, struct task_struct *p,
			     int oldprio, int running)
{
	check_deadline_preempt_curr(p, rq);
}

static void switched_to_deadline(struct rq *rq, struct task_struct *p,
			    int running)
{
	check_deadline_preempt_curr(p, rq);
}

#ifdef CONFIG_SMP
static int select_task_rq_deadline(struct task_struct *p, int sync)
{
	return task_cpu(p);
}

static unsigned long
load_balance_deadline(struct rq *this_rq, int this_cpu, struct rq *busiest,
		 unsigned long max_load_move,
		 struct sched_domain *sd, enum cpu_idle_type idle,
		 int *all_pinned, int *this_best_prio)
{
	/* for now, don't touch SCHED_DEADLINE tasks */
	return 0;
}

static int
move_one_task_deadline(struct rq *this_rq, int this_cpu, struct rq *busiest,
		  struct sched_domain *sd, enum cpu_idle_type idle)
{
	return 0;
}

static void set_cpus_allowed_deadline(struct task_struct *p,
				 const struct cpumask *new_mask)
{
	int weight = cpumask_weight(new_mask);

	BUG_ON(!deadline_task(p));

	cpumask_copy(&p->cpus_allowed, new_mask);
	p->dl.nr_cpus_allowed = weight;
}
#endif

static const struct sched_class deadline_sched_class = {
	.next			= &rt_sched_class,
	.enqueue_task		= enqueue_task_deadline,
	.dequeue_task		= dequeue_task_deadline,
	.yield_task		= yield_task_deadline,
	.wait_interval		= wait_interval_deadline,

	.check_preempt_curr	= check_preempt_curr_deadline,

	.pick_next_task		= pick_next_task_deadline,
	.put_prev_task		= put_prev_task_deadline,

#ifdef CONFIG_SMP
	.select_task_rq		= select_task_rq_deadline,

	.load_balance           = load_balance_deadline,
	.move_one_task		= move_one_task_deadline,
	.set_cpus_allowed       = set_cpus_allowed_deadline,
#endif

	.set_curr_task		= set_curr_task_deadline,
	.task_tick		= task_tick_deadline,

	.prio_changed           = prio_changed_deadline,
	.switched_to		= switched_to_deadline,
};

#ifdef CONFIG_SCHED_DEBUG
static void print_deadline_stats(struct seq_file *m, int cpu)
{
	struct dl_rq *dl_rq = &cpu_rq(cpu)->dl;

	rcu_read_lock();
	for_each_leaf_deadline_rq(dl_rq, cpu_rq(cpu))
		print_deadline_rq(m, cpu, dl_rq);
	rcu_read_unlock();
}
#endif /* CONFIG_SCHED_DEBUG */
