/****************************************************************************
 *
 * Copyright 2016 Samsung Electronics All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
 * either express or implied. See the License for the specific
 * language governing permissions and limitations under the License.
 *
 ****************************************************************************/
/****************************************************************************
 * kernel/task/task_activate.c
 *
 *   Copyright (C) 2007-2009 Gregory Nutt. All rights reserved.
 *   Author: Gregory Nutt <gnutt@nuttx.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name NuttX nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <tinyara/config.h>

#include <sched.h>
#include <debug.h>

#include <tinyara/arch.h>
#ifndef CONFIG_DISABLE_SIGNALS
#include <pthread.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <tinyara/signal.h>
#include "sched/sched.h"
#endif

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/****************************************************************************
 * Private Type Declarations
 ****************************************************************************/

/****************************************************************************
 * Global Variables
 ****************************************************************************/

/****************************************************************************
 * Private Variables
 ****************************************************************************/

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

/****************************************************************************
 * Private Functions
 ****************************************************************************/
#ifndef CONFIG_DISABLE_SIGNALS
void thread_termination_handler(int signo, siginfo_t *data)
{
	struct tcb_s *rtcb = sched_gettcb(getpid());
	if (!rtcb) {
		set_errno(ESRCH);
		return;
	}

#ifdef CONFIG_SIGKILL_HANDLER
	if (rtcb->sigkillusrhandler != NULL) {
		rtcb->sigkillusrhandler(signo, data, NULL);
	}
#endif

	switch ((rtcb->flags & TCB_FLAG_TTYPE_MASK) >> TCB_FLAG_TTYPE_SHIFT) {
	case TCB_FLAG_TTYPE_TASK:
	case TCB_FLAG_TTYPE_KERNEL:
		/* tasks and kernel threads has to use this interface */
		(void)task_delete(rtcb->pid);
		break;
#ifndef CONFIG_DISABLE_PTHREAD
	case TCB_FLAG_TTYPE_PTHREAD:
		(void)pthread_cancel(rtcb->pid);
		(void)pthread_join(rtcb->pid, NULL);
		break;
#endif
	default:
		set_errno(EINVAL);
		break;
	}
	return;
}
#endif

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: task_activate
 *
 * Description:
 *   This function activates tasks initialized by task_schedsetup(). Without
 *   activation, a task is ineligible for execution by the scheduler.
 *
 * Input Parameters:
 *   tcb - The TCB for the task for the task (same as the task_init argument).
 *
 * Return Value:
 *   Always returns OK
 *
 ****************************************************************************/

int task_activate(FAR struct tcb_s *tcb)
{
	irqstate_t flags = irqsave();
#ifndef CONFIG_DISABLE_SIGNALS
	int ret;
	struct sigaction act;

	act.sa_sigaction = (_sa_sigaction_t)thread_termination_handler;
	act.sa_flags = 0;
	(void)sigemptyset(&act.sa_mask);

	ret = sig_sethandler(tcb, SIGKILL, &act);
	if (ret != OK) {
		sdbg("Fail to set SIGKILL handler for activating tcb.\n");
	}
#endif
	up_unblock_task(tcb);
	irqrestore(flags);
	return OK;
}
