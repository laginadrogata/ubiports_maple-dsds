/*
 * Copyright (c) 2014, 2016, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/errno.h>
#include <linux/smp.h>
#include <linux/cpu.h>
#include <linux/cpumask.h>
#include <linux/slab.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/printk.h>
#include <linux/sched.h>
#include <linux/workqueue.h>
#include <linux/atomic.h>

#define MODULE_NAME "msm_ipi_test"
#define PRINT_BUFF_MAXSIZE 4096

static DECLARE_WAIT_QUEUE_HEAD(wrk_sts);

struct ipi_work_data {
	struct work_struct __percpu *work;
	atomic_t cpu_ack;
	bool single;
};

struct ipi_tx_data {
	long long tx_time;
	u32 tx_cpu;
	bool single;	/*for smp_call_function_single ?? */
};

static struct ipi_work_data work_data;

static int ipi_test_array[2][NR_CPUS][NR_CPUS];
static int ipi_max_time[2][NR_CPUS][NR_CPUS];

static int ipi_test_iteration = 10000;
static int ipi_maxtime;
atomic_t work_start;

/*IPI handler */
static void ipi_test_call(void *data)
{
	long nsec, prev;
	u32 i;
	struct ipi_tx_data *txd = data;
	long long rxt, txt = txd->tx_time;
	bool sg = txd->single;

	/*Calculate latency*/
	rxt = sched_clock();
	nsec = rxt - txt;
	i = raw_smp_processor_id();

	prev = ipi_max_time[sg][txd->tx_cpu][i];
	ipi_max_time[sg][txd->tx_cpu][i] = (prev > nsec) ? prev : nsec;
	ipi_test_array[sg][txd->tx_cpu][i]++;

	pr_debug("IPI Test: %d rx_cpu %d tx_cpu %d  prev=%ld new = %ld\n",
			(int)txd->single, i, txd->tx_cpu, prev, nsec);
}

static void ipi_test_work(struct work_struct *work)
{
	int k = 0, i;

	/*Let's wait for work to schedule on all cpus*/
	do {
		cpu_relax();
	} while (!atomic_read(&work_start));

	for (i = 0; i < ipi_test_iteration; i++) {
		struct ipi_tx_data *txd = kmalloc(sizeof(struct ipi_tx_data), GFP_KERNEL);
		if (!txd) {
			pr_err("IPI Test: CPU: %d Unable to allocate memory \
					for tx data iter: %i\n", smp_processor_id(), i);
			wake_up_interruptible(&wrk_sts);
			return;
		}

		txd->tx_cpu =  raw_smp_processor_id();
		txd->tx_time = sched_clock();

		if (work_data.single) {
			txd->single = true;		/*send IPI one by one*/
			for_each_online_cpu(k) {
				if (k == smp_processor_id())
					continue;
				smp_call_function_single(k, ipi_test_call, (void *)txd, true);
				}
		} else {
			txd->single = false;
			smp_call_function(ipi_test_call, (void *)txd, true);
		}

		kfree(txd);
	}

	/* Done on this cpu, decrement cpu_ack count */
	if (atomic_dec_and_test(&work_data.cpu_ack))
		wake_up_interruptible(&wrk_sts);
}

static int ipi_test_run(bool val)
{
	int cpu, i, j, err = 0;
	int max_val = ipi_maxtime;

	/* Make sure no cpu goes offline during the test */
	get_online_cpus();

	atomic_set(&work_data.cpu_ack, num_online_cpus());
	atomic_set(&work_start, 0);

	work_data.work = alloc_percpu(struct work_struct);
	if (!work_data.work)
		return -ENOMEM;

	/* Initialize and schedule ipi work item on each cpu */
	for_each_online_cpu(cpu) {
		struct work_struct *work = per_cpu_ptr(work_data.work, cpu);

		INIT_WORK(work, ipi_test_work);
		work_data.single = val;
		schedule_work_on(cpu, work);
	}

	/* Let's start work on all cpu at once*/
	atomic_set(&work_start, 1);

	/* Wait for ACK from all cpus (max 4 mins) */
	if (!wait_event_interruptible_timeout(wrk_sts,
			atomic_read(&work_data.cpu_ack) == 0, msecs_to_jiffies(240000)))
		pr_err("Timeout on cpu ACK live mask 0x%X !!!\n",
					atomic_read(&work_data.cpu_ack));

	for_each_online_cpu(i) {
		for_each_online_cpu(j) {
			pr_debug("IPI Test: CPU %d -> %d: %d\n", i, j, ipi_test_array[val][i][j]);

			if (i != j && (ipi_test_array[val][i][j]) != ipi_test_iteration) {
				pr_alert("IPI Test: Erred tx cpu = %d rx cpu = %d val = %d\n",
							i, j, ipi_test_array[val][i][j]);
				err = -ERANGE;
			}
			max_val = (max_val > ipi_max_time[val][i][j]) ?
						max_val : ipi_max_time[val][i][j];
		}
	}

	put_online_cpus();
	free_percpu(work_data.work);

	ipi_maxtime = max_val;

	return err;
}

static int set_ipitest_start(void *data, u64 val)
{
	int ret = 0;

#ifdef CONFIG_HOTPLUG_CPU
	int cpu;

	for_each_present_cpu(cpu) {
		if (!cpu_online(cpu)) {
			ret = cpu_up(cpu);
			if (ret) {
				pr_err("IPI Test: failed to bring up cpu%d!\n", cpu);
				return ret;
			}
		}
	}
#endif
	memset(ipi_test_array, 0, sizeof(ipi_test_array));
	memset(ipi_max_time, 0, sizeof(ipi_max_time));

	switch (val) {
	case 1:
		ret = ipi_test_run(0);
		break;
	case 2:
		ret = ipi_test_run(1);
		break;
	case 3:
		ret = ipi_test_run(0);
		if (ret)
			break;
		ret = ipi_test_run(1);
		break;
	default:
		pr_err("IPI Test:Invalid test type\n");
		ret = -EINVAL;
	}
	if (!ret) {
		pr_debug("IPI Test: Result: PASS\n");
		pr_debug("IPI Test: Number of IPI's per CPU %d\n", ipi_test_iteration);
		pr_debug("IPI Test: Max Time taken to handle IPI:%d (nsec)\n", ipi_maxtime);
	} else
		pr_err("IPI Test: Result: FAILED\n");

	return ret;
}

static int get_ipitest_maxtime(void *data, u64 *val)
{
	*val = ipi_maxtime;
	return 0;
}

static int set_ipitest_iteration(void *data, u64 val)
{
	if (val > 0)
		ipi_test_iteration = val;
	else
		return -1;

	return 0;
}

static int get_ipitest_iteration(void *data, u64 *val)
{
	*val = ipi_test_iteration;

	return 0;
}

static ssize_t ipitest_read(struct file *file,
		char __user *user_buf, size_t count, loff_t *ppos)
{
	char *buff;
	int desc, i, j, k, ret;

	buff = kmalloc(PRINT_BUFF_MAXSIZE, GFP_KERNEL);
	if (!buff)
		return -ENOMEM;

	desc += snprintf(buff+desc, 32, "Time taken to handle IPI's:\n");

	for (k = 0; k < 2; k++) {
		desc += snprintf(buff+desc, 24, "Test type IPI %s\n", k ? "Many" : "Single");
		desc += snprintf(buff+desc, 80,
				"\tCPU0  |  CPU1  |  CPU2  |  CPU3  |  CPU4  | CPU5  | CPU6  |  CPU7 \n");

		for (i = 0; i < NR_CPUS; i++) {
			desc += snprintf(buff+desc, 6, "CPU%d ", i);
			for (j = 0; j < NR_CPUS; j++)
				desc += snprintf(buff+desc, 12, "%8d ", ipi_max_time[k][i][j]);
			desc += snprintf(buff+desc, 2, "\n");
		}

		desc += snprintf(buff+desc, 12, "IPI Nos:\n");
		for (i = 0; i < NR_CPUS; i++) {
			for (j = 0; j < NR_CPUS; j++)
				desc += snprintf(buff+desc, 12, "%4d ", ipi_test_array[k][i][j]);
			desc += snprintf(buff+desc, 2, "\n");
		}
	}

	ret = simple_read_from_buffer(user_buf, count, ppos, buff, desc);
	kfree(buff);

	return ret;
}

static const struct file_operations ipitest_result_fops = {
	.read = ipitest_read,
	.open = simple_open,
	.llseek = default_llseek,
};

DEFINE_SIMPLE_ATTRIBUTE(ipitest_start_fops, get_ipitest_maxtime,
			set_ipitest_start, "%llu\n");
DEFINE_SIMPLE_ATTRIBUTE(ipitest_iteration_fops, get_ipitest_iteration,
			set_ipitest_iteration, "%llu\n");

static struct dentry *ipi_test_dir_dentry;

static int __init ipi_test_init(void)
{
	struct dentry *ipi_test_dentry;

	ipi_test_dir_dentry = debugfs_create_dir("ipi_test", 0);
	if (!ipi_test_dir_dentry) {
		pr_err("IPI Test:Failed to create the debugfs ipi_test file\n");
		goto out;
	}

	ipi_test_dentry = debugfs_create_file("iteration", 0644,
			ipi_test_dir_dentry, NULL, &ipitest_iteration_fops);
	if (!ipi_test_dentry) {
		pr_err("IPI Test:Failed to create the debugfs iteration entry\n");
		goto out_dir;
	}

	ipi_test_dentry = debugfs_create_file("start", 0644,
			ipi_test_dir_dentry, NULL, &ipitest_start_fops);
	if (!ipi_test_dentry) {
		pr_err("IPI Test:Failed to create the debugfs start entry\n");
		goto out_dir;
	}

	ipi_test_dentry = debugfs_create_file("result", 0644,
			ipi_test_dir_dentry, NULL, &ipitest_result_fops);
	if (!ipi_test_dentry) {
		pr_err("IPI Test:Failed to create the debugfs result entry\n");
		goto out_dir;
	}

	return 0;
out_dir:
	debugfs_remove_recursive(ipi_test_dir_dentry);
out:
	return -ENODEV;
}

static void __exit ipi_test_exit(void)
{
	debugfs_remove_recursive(ipi_test_dir_dentry);
}

module_init(ipi_test_init);
module_exit(ipi_test_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MSM IPI test");
