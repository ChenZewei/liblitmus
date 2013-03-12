#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>


#include <sched.h> /* for cpu sets */

#include "litmus.h"
#include "internal.h"

#define LP(name) {name ## _SEM, #name}

static struct {
	int id;
	const char* name;
} protocol[] = {
	LP(FMLP),
	LP(SRP),
	LP(MPCP),
	LP(MPCP_VS),
	{MPCP_VS_SEM, "MPCP-VS"},
	LP(DPCP),
	LP(PCP),
};

#define NUM_PROTOS (sizeof(protocol)/sizeof(protocol[0]))

int lock_protocol_for_name(const char* name)
{
	int i;

	for (i = 0; i < NUM_PROTOS; i++)
		if (strcmp(name, protocol[i].name) == 0)
			return protocol[i].id;

	return -1;
}

const char* name_for_lock_protocol(int id)
{
	int i;

	for (i = 0; i < NUM_PROTOS; i++)
		if (protocol[i].id == id)
			return protocol[i].name;

	return "<UNKNOWN>";
}

int litmus_open_lock(
	obj_type_t protocol,
	int lock_id,
	const char* namespace,
	void *config_param)
{
	int fd, od;

	fd = open(namespace, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
	if (fd < 0)
		return -1;
	od = od_openx(fd, protocol, lock_id, config_param);
	close(fd);
	return od;
}



void show_rt_param(struct rt_task* tp)
{
	printf("rt params:\n\t"
	       "exec_cost:\t%llu\n\tperiod:\t\t%llu\n\tcpu:\t%d\n",
	       tp->exec_cost, tp->period, tp->cpu);
}

void init_rt_task_param(struct rt_task* tp)
{
	/* Defaults:
	 *  - implicit deadline (t->relative_deadline == 0)
	 *  - phase = 0
	 *  - class = RT_CLASS_SOFT
	 *  - budget policy = NO_ENFORCEMENT
	 *  - fixed priority = LITMUS_LOWEST_PRIORITY
	 *  - release policy = SPORADIC
	 *  - cpu assignment = 0
	 *
	 * User must still set the following fields to non-zero values:
	 *  - tp->exec_cost
	 *  - tp->period
	 *
	 * User must set tp->cpu to the appropriate value for non-global
	 * schedulers. For clusters, set tp->cpu to the first CPU in the
	 * assigned cluster.
	 */

	memset(tp, 0, sizeof(*tp));

	tp->cls = RT_CLASS_SOFT;
	tp->priority = LITMUS_LOWEST_PRIORITY;
	tp->budget_policy = NO_ENFORCEMENT;
	tp->release_policy = SPORADIC;
}

task_class_t str2class(const char* str)
{
	if      (!strcmp(str, "hrt"))
		return RT_CLASS_HARD;
	else if (!strcmp(str, "srt"))
		return RT_CLASS_SOFT;
	else if (!strcmp(str, "be"))
		return RT_CLASS_BEST_EFFORT;
	else
		return -1;
}

#define NS_PER_MS 1000000

int sporadic_task(lt_t e, lt_t p, lt_t phase,
		  int cluster, int cluster_size, unsigned int priority,
		  task_class_t cls,
		  budget_policy_t budget_policy, int set_cpu_set)
{
	return sporadic_task_ns(e * NS_PER_MS, p * NS_PER_MS, phase * NS_PER_MS,
				cluster, cluster_size, priority, cls,
				budget_policy, set_cpu_set);
}

int sporadic_task_ns(lt_t e, lt_t p, lt_t phase,
		     int cluster, int cluster_size, unsigned int priority,
		     task_class_t cls,
		     budget_policy_t budget_policy, int migrate)
{
	struct rt_task param;
	int ret;

	/* Zero out first --- this is helpful when we add plugin-specific
	 * parameters during development.
	 */
	memset(&param, 0, sizeof(param));

	param.exec_cost = e;
	param.period    = p;
	param.relative_deadline = p; /* implicit deadline */
	param.cpu       = cluster_to_first_cpu(cluster, cluster_size);
	param.cls       = cls;
	param.phase	= phase;
	param.budget_policy = budget_policy;
	param.priority  = priority;

	if (migrate) {
		ret = be_migrate_to_cluster(cluster, cluster_size);
		check("migrate to cluster");
	}
	return set_rt_task_param(gettid(), &param);
}

int init_kernel_iface(void);

int init_litmus(void)
{
	int ret, ret2;

	ret = mlockall(MCL_CURRENT | MCL_FUTURE);
	check("mlockall()");
	ret2 = init_rt_thread();
	return (ret == 0) && (ret2 == 0) ? 0 : -1;
}

int init_rt_thread(void)
{
	int ret;

        ret = init_kernel_iface();
	check("kernel <-> user space interface initialization");
	return ret;
}

void exit_litmus(void)
{
	/* nothing to do in current version */
}
