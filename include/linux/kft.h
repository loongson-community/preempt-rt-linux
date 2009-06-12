#ifndef _LINUX_KFT_H
#define _LINUX_KFT_H

#define KFT_MODE_TIMED 		0x01
#define KFT_MODE_AUTO_REPEAT 	0x02
#define KFT_MODE_STOP_ON_FULL 	0x04

#define TIME_NOT_SET	0xffffffff

typedef struct kft_entry {
	void *va;            /* VA of instrumented function */
	void *call_site;     /* where this func was called */
	unsigned long time;  /* function entry time since trigger start time,
				in usec */
	unsigned long delta; /* delta time from entry to exit, in usec */
	int           pid;
#ifdef CONFIG_KFT_SAVE_ARGS
	unsigned long fp;    /* frame pointer address */
	unsigned long a1;    /* first argument passed */
	unsigned long a2;    /* second argument passed */
	unsigned long a3;    /* third argument passed */
#endif /* CONFIG_KFT_SAVE_ARGS */
} kft_entry_t;

#define INTR_CONTEXT -1

#define TRIGGER_START_ON_ENTRY	0x01
#define TRIGGER_START_ON_EXIT	0x02
#define TRIGGER_STOP_ON_ENTRY	0x04
#define TRIGGER_STOP_ON_EXIT	0x08

typedef enum kft_trigger_type {
	TRIGGER_NONE = 0,
	TRIGGER_TIME,
	TRIGGER_FUNC_ENTRY,
	TRIGGER_FUNC_EXIT,
	TRIGGER_PROC,
	TRIGGER_USER,
	TRIGGER_LOG_FULL
} kft_trigger_type_t;

typedef struct kft_trigger {
	enum kft_trigger_type type;
	union {
		unsigned long time; /*  time since boot, in usec */
		void *func_addr;
	};
	unsigned long mark; /*  time at which this trigger occured */
} kft_trigger_t;

#define DEFAULT_RUN_LOG_ENTRIES 20000
#define MAX_FUNC_LIST_ENTRIES 512

typedef struct kft_filters {
	unsigned long min_delta;
	unsigned long max_delta;
	int no_ints;
	int only_ints;
	void **func_list;
	int func_list_size;
	struct {
		int delta;
		int no_ints;
		int only_ints;
		int func_list;
	} cnt;
} kft_filters_t;

typedef struct kft_run {
	int primed;	/* is this run ready to start */
	int triggered;	/* has this run started */
	int complete;	/* has this run ended */
	int flags;
	/* int trigger_flag; */
	struct kft_trigger start_trigger;
	struct kft_trigger stop_trigger;
	struct kft_filters filters;
	struct kft_entry *log;
	int log_is_kmem;
	int num_entries;
	int next_entry;
	int id;
	int notfound;
} kft_run_t;

#if CONFIG_KFT_CLOCK_SCALE
extern void setup_early_kft_clock(void);
#else
#define setup_early_kft_clock()
#endif

extern const struct seq_operations kft_data_op;
extern int kfi_dump_log(char *buf);

#endif /* _LINUX_KFT_H */
