#ifdef CONFIG_CPU_LOONGSON2

#define LOONGSON2_PERFCTRL_EVENT_WHENEVER		\
	(LOONGSON2_PERFCTRL_EXL | LOONGSON2_PERFCTRL_KERNEL |	\
	LOONGSON2_PERFCTRL_USER | LOONGSON2_PERFCTRL_SUPERVISOR |	\
	LOONGSON2_PERFCTRL_ENABLE)

#define LOONGSON2_PERFCTRL_CONFIG_MASK 0x1f

static inline unsigned int
loongson2_pmu_read_counter(unsigned int idx)
{
	uint64_t counter = read_c0_perfcnt();

	switch (idx) {
	case 0:
		return counter & 0xffffffff;
	case 1:
		return counter >> 32;
	default:
		WARN_ONCE(1, "Invalid performance counter number (%d)\n", idx);
		return 0;
	}
}

static inline void
loongson2_pmu_write_counter(unsigned int idx, unsigned int val)
{
	uint64_t counter = read_c0_perfcnt();

	switch (idx) {
	case 0:
		write_c0_perfcnt(val | counter);
		return;
	case 1:
		write_c0_perfcnt(((uint64_t)val << 32) | counter);
		return;
	}
}

static inline unsigned int
loongson2_pmu_read_control(unsigned int idx)
{
	return read_c0_perfctrl();
}

static inline void
loongson2_pmu_write_control(unsigned int idx, unsigned int val)
{
	write_c0_perfctrl(val);
	return;
}

static const struct mips_perf_event loongson2_event_map
				[PERF_COUNT_HW_MAX] = {
	[PERF_COUNT_HW_CPU_CYCLES] = { 0x00, CNTR_EVEN },
	[PERF_COUNT_HW_INSTRUCTIONS] = { 0x00, CNTR_ODD },
	[PERF_COUNT_HW_CACHE_REFERENCES] = { UNSUPPORTED_PERF_EVENT_ID },
	[PERF_COUNT_HW_CACHE_MISSES] = { UNSUPPORTED_PERF_EVENT_ID },
	[PERF_COUNT_HW_BRANCH_INSTRUCTIONS] = { 0x01, CNTR_EVEN },
	[PERF_COUNT_HW_BRANCH_MISSES] = { 0x01, CNTR_ODD },
	[PERF_COUNT_HW_BUS_CYCLES] = { UNSUPPORTED_PERF_EVENT_ID },
};

static const struct mips_perf_event loongson2_cache_map
				[PERF_COUNT_HW_CACHE_MAX]
				[PERF_COUNT_HW_CACHE_OP_MAX]
				[PERF_COUNT_HW_CACHE_RESULT_MAX] = {
[C(L1D)] = {
	/*
	 * Like some other architectures (e.g. ARM), the performance
	 * counters don't differentiate between read and write
	 * accesses/misses, so this isn't strictly correct, but it's the
	 * best we can do. Writes and reads get combined.
	 */
	[C(OP_READ)] = {
		[C(RESULT_ACCESS)]	= { UNSUPPORTED_PERF_EVENT_ID },
		[C(RESULT_MISS)]	= { 0x04, CNTR_ODD },
	},
	[C(OP_WRITE)] = {
		[C(RESULT_ACCESS)]	= { UNSUPPORTED_PERF_EVENT_ID },
		[C(RESULT_MISS)]	= { 0x04, CNTR_ODD },
	},
	[C(OP_PREFETCH)] = {
		[C(RESULT_ACCESS)]	= { UNSUPPORTED_PERF_EVENT_ID },
		[C(RESULT_MISS)]	= { UNSUPPORTED_PERF_EVENT_ID },
	},
},
[C(L1I)] = {
	[C(OP_READ)] = {
		[C(RESULT_ACCESS)]	= { UNSUPPORTED_PERF_EVENT_ID },
		[C(RESULT_MISS)]	= { 0x04, CNTR_EVEN },
	},
	[C(OP_WRITE)] = {
		[C(RESULT_ACCESS)]	= { UNSUPPORTED_PERF_EVENT_ID },
		[C(RESULT_MISS)]	= { 0x04, CNTR_EVEN },
	},
	[C(OP_PREFETCH)] = {
		[C(RESULT_ACCESS)]	= { UNSUPPORTED_PERF_EVENT_ID },
		/*
		 * Note that MIPS has only "hit" events countable for
		 * the prefetch operation.
		 */
		[C(RESULT_MISS)]	= { UNSUPPORTED_PERF_EVENT_ID },
	},
},
[C(DTLB)] = {
	[C(OP_READ)] = {
		[C(RESULT_ACCESS)]	= { UNSUPPORTED_PERF_EVENT_ID },
		[C(RESULT_MISS)]	= { 0x0d, CNTR_EVEN },
	},
	[C(OP_WRITE)] = {
		[C(RESULT_ACCESS)]	= { UNSUPPORTED_PERF_EVENT_ID },
		[C(RESULT_MISS)]	= { 0x0d, CNTR_EVEN },
	},
	[C(OP_PREFETCH)] = {
		[C(RESULT_ACCESS)]	= { UNSUPPORTED_PERF_EVENT_ID },
		[C(RESULT_MISS)]	= { UNSUPPORTED_PERF_EVENT_ID },
	},
},
[C(ITLB)] = {
	[C(OP_READ)] = {
		[C(RESULT_ACCESS)]	= { UNSUPPORTED_PERF_EVENT_ID },
		[C(RESULT_MISS)]	= { 0x0c, CNTR_ODD },
	},
	[C(OP_WRITE)] = {
		[C(RESULT_ACCESS)]	= { UNSUPPORTED_PERF_EVENT_ID },
		[C(RESULT_MISS)]	= { 0x0c, CNTR_ODD },
	},
	[C(OP_PREFETCH)] = {
		[C(RESULT_ACCESS)]	= { UNSUPPORTED_PERF_EVENT_ID },
		[C(RESULT_MISS)]	= { UNSUPPORTED_PERF_EVENT_ID },
	},
},
};

static int __hw_perf_event_init(struct perf_event *event)
{
	struct perf_event_attr *attr = &event->attr;
	struct hw_perf_event *hwc = &event->hw;
	const struct mips_perf_event *pev;
	int err;

	/* Returning MIPS event descriptor for generic perf event. */
	if (PERF_TYPE_HARDWARE == event->attr.type) {
		if (event->attr.config >= PERF_COUNT_HW_MAX)
			return -EINVAL;
		pev = mipspmu_map_general_event(event->attr.config);
	} else if (PERF_TYPE_HW_CACHE == event->attr.type) {
		pev = mipspmu_map_cache_event(event->attr.config);
	} else if (PERF_TYPE_RAW == event->attr.type) {
		/* We are working on the global raw event. */
		mutex_lock(&raw_event_mutex);
		pev = mipspmu->map_raw_event(event->attr.config);
	} else {
		/* The event type is not (yet) supported. */
		return -EOPNOTSUPP;
	}

	if (IS_ERR(pev)) {
		if (PERF_TYPE_RAW == event->attr.type)
			mutex_unlock(&raw_event_mutex);
		return PTR_ERR(pev);
	}

	/*
	 * We allow max flexibility on how each individual counter shared
	 * by the single CPU operates (the mode exclusion and the range).
	 */
	hwc->config_base = LOONGSON2_PERFCTRL_ENABLE;

	hwc->event_base = mipspmu_perf_event_encode(pev);
	if (PERF_TYPE_RAW == event->attr.type)
		mutex_unlock(&raw_event_mutex);

	if (!attr->exclude_user)
		hwc->config_base |= LOONGSON2_PERFCTRL_USER;
	if (!attr->exclude_kernel) {
		hwc->config_base |= LOONGSON2_PERFCTRL_KERNEL;
		/* MIPS kernel mode: KSU == 00b || EXL == 1 || ERL == 1 */
		hwc->config_base |= LOONGSON2_PERFCTRL_EXL;
	}
	if (!attr->exclude_hv)
		hwc->config_base |= LOONGSON2_PERFCTRL_SUPERVISOR;

	hwc->config_base &= LOONGSON2_PERFCTRL_CONFIG_MASK;
	/*
	 * The event can belong to another cpu. We do not assign a local
	 * counter for it for now.
	 */
	hwc->idx = -1;
	hwc->config = 0;

	if (!hwc->sample_period) {
		hwc->sample_period  = MAX_PERIOD;
		hwc->last_period    = hwc->sample_period;
		atomic64_set(&hwc->period_left, hwc->sample_period);
	}

	err = 0;
	if (event->group_leader != event) {
		err = validate_group(event);
		if (err)
			return -EINVAL;
	}

	event->destroy = hw_perf_event_destroy;

	return err;
}

static void pause_local_counters(void)
{
	struct cpu_hw_events *cpuc = &__get_cpu_var(cpu_hw_events);
	unsigned long flags;

	local_irq_save(flags);
	cpuc->saved_ctrl[0] = read_c0_perfctrl();
	write_c0_perfctrl(cpuc->saved_ctrl[0] &
		~LOONGSON2_PERFCTRL_EVENT_WHENEVER);
	local_irq_restore(flags);
}

static void resume_local_counters(void)
{
	struct cpu_hw_events *cpuc = &__get_cpu_var(cpu_hw_events);
	unsigned long flags;

	local_irq_save(flags);
	write_c0_perfctrl(cpuc->saved_ctrl[0]);
	local_irq_restore(flags);
}

static int loongson2_pmu_handle_shared_irq(void)
{
	struct cpu_hw_events *cpuc = &__get_cpu_var(cpu_hw_events);
	struct perf_sample_data data;
	unsigned int counters = mipspmu->num_counters;
	unsigned int counter;
	int handled = IRQ_NONE;
	struct pt_regs *regs;

	/* Check whether the irq belongs to me */
	if (!(read_c0_perfcnt() & LOONGSON2_PERFCTRL_ENABLE))
		return IRQ_NONE;

	/*
	 * First we pause the local counters, so that when we are locked
	 * here, the counters are all paused. When it gets locked due to
	 * perf_disable(), the timer interrupt handler will be delayed.
	 *
	 * See also loongson2_pmu_start().
	 */
	pause_local_counters();

	regs = get_irq_regs();

	perf_sample_data_init(&data, 0);

	switch (counters) {
#define HANDLE_COUNTER(n)						\
	case n + 1:							\
		if (test_bit(n, cpuc->used_mask)) {			\
			counter = loongson2_pmu_read_counter(n);	\
			if (counter & LOONGSON2_PERFCNT_OVERFLOW) {	\
				loongson2_pmu_write_counter(n, counter &\
						0x7fffffff);		\
				if (test_and_change_bit(n, cpuc->msbs))	\
					handle_associated_event(cpuc,	\
						n, &data, regs);	\
				handled = IRQ_HANDLED;			\
			}						\
		}
	HANDLE_COUNTER(1)
	HANDLE_COUNTER(0)
	}

	/*
	 * Do all the work for the pending perf events. We can do this
	 * in here because the performance counter interrupt is a regular
	 * interrupt, not NMI.
	 */
	if (handled == IRQ_HANDLED)
		perf_event_do_pending();

	resume_local_counters();
	return handled;
}

static irqreturn_t
loongson2_pmu_handle_irq(int irq, void *dev)
{
	return loongson2_pmu_handle_shared_irq();
}

static void loongson2_pmu_start(void)
{
	resume_local_counters();
}

static void loongson2_pmu_stop(void)
{
	pause_local_counters();
}

static int
loongson2_pmu_alloc_counter(struct cpu_hw_events *cpuc,
			struct hw_perf_event *hwc)
{
	int i;

	/*
	 * We only need to care the counter mask. The range has been
	 * checked definitely.
	 */
	unsigned long cntr_mask = (hwc->event_base >> 8) & 0xffff;

	for (i = mipspmu->num_counters - 1; i >= 0; i--) {
		/*
		 * Note that some MIPS perf events can be counted by both
		 * even and odd counters, wheresas many other are only by
		 * even _or_ odd counters. This introduces an issue that
		 * when the former kind of event takes the counter the
		 * latter kind of event wants to use, then the "counter
		 * allocation" for the latter event will fail. In fact if
		 * they can be dynamically swapped, they both feel happy.
		 * But here we leave this issue alone for now.
		 */
		if (test_bit(i, &cntr_mask) &&
			!test_and_set_bit(i, cpuc->used_mask))
			return i;
	}

	return -EAGAIN;
}

static void
loongson2_pmu_enable_event(struct hw_perf_event *evt, int idx)
{
	struct cpu_hw_events *cpuc = &__get_cpu_var(cpu_hw_events);
	unsigned long flags;

	WARN_ON(idx < 0 || idx >= mipspmu->num_counters);

	local_irq_save(flags);

	cpuc->saved_ctrl[idx] =
		LOONGSON2_PERFCTRL_EVENT(idx, evt->event_base & 0xff) |
		(evt->config_base & LOONGSON2_PERFCTRL_CONFIG_MASK) |
		/* Make sure interrupt enabled. */
		LOONGSON2_PERFCTRL_ENABLE;
	/*
	 * We do not actually let the counter run. Leave it until start().
	 */
	local_irq_restore(flags);
}

static void
loongson2_pmu_disable_event(int idx)
{
	struct cpu_hw_events *cpuc = &__get_cpu_var(cpu_hw_events);
	unsigned long flags;

	WARN_ON(idx < 0 || idx >= mipspmu->num_counters);

	local_irq_save(flags);
	cpuc->saved_ctrl[idx] = loongson2_pmu_read_control(idx) &
		~LOONGSON2_PERFCTRL_EVENT_WHENEVER;
	loongson2_pmu_write_control(idx, cpuc->saved_ctrl[idx]);
	local_irq_restore(flags);
}

/*
 * User can use 0-255 raw events, where 0-127 for the events of even
 * counters, and 128-255 for odd counters. Note that bit 7 is used to
 * indicate the parity. So, for example, when user wants to take the
 * Event Num of 15 for odd counters (by referring to the user manual),
 * then 128 needs to be added to 15 as the input for the event config,
 * i.e., 143 (0x8F) to be used.
 */
static const struct mips_perf_event *
loongson2_pmu_map_raw_event(u64 config)
{
	unsigned int raw_id = config & 0xff;
	unsigned int base_id = raw_id & 0x7f;

	raw_event.event_id = base_id;
	raw_event.cntr_mask = raw_id > 127 ? CNTR_ODD : CNTR_EVEN;

	return &raw_event;
}

static struct mips_pmu loongson2_pmu = {
	.handle_irq = loongson2_pmu_handle_irq,
	.handle_shared_irq = loongson2_pmu_handle_shared_irq,
	.start = loongson2_pmu_start,
	.stop = loongson2_pmu_stop,
	.alloc_counter = loongson2_pmu_alloc_counter,
	.read_counter = loongson2_pmu_read_counter,
	.write_counter = loongson2_pmu_write_counter,
	.enable_event = loongson2_pmu_enable_event,
	.disable_event = loongson2_pmu_disable_event,
	.map_raw_event = loongson2_pmu_map_raw_event,
	.general_event_map = &loongson2_event_map,
	.cache_event_map = &loongson2_cache_map,
};

static int __init
init_hw_perf_events(void)
{
	pr_info("Performance counters: ");

	reset_counters(NULL);

	loongson2_pmu.name = LOONGSON2_CPU_TYPE;
	loongson2_pmu.num_counters = 2;
	mipspmu = &loongson2_pmu;

	if (mipspmu)
		pr_cont("%s PMU enabled, %d counters available to each "
			"CPU\n", mipspmu->name, mipspmu->num_counters);

	return 0;
}
arch_initcall(init_hw_perf_events);

#endif
