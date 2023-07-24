#include <types.h>
#include <kern/errno.h>
#include <kern/unistd.h>
#include <kern/wait.h>
#include <lib.h>
#include <syscall.h>
#include <current.h>
#include <proc.h>
#include <thread.h>
#include <addrspace.h>
#include <copyinout.h>
#include <synch.h> 
#include <mips/trapframe.h> 
#include <kern/fcntl.h>
#include <vfs.h> 
#include <vm.h> 
#include "opt-A2.h"


int
sys_execv(userptr_t progname, char ** args)
{
	struct addrspace *as;
	struct vnode *v;
	vaddr_t entrypoint, stackptr;
	int result;	
	int n = 0;
	char *kernel[n+1];

	for (; args[n] != NULL; ++n) {}

	for (int i = 0; i < n; ++i) {
      		int size = strlen(args[i])+1;
	  	kernel[i] = kmalloc(size);
	  	KASSERT(kernel[i] != NULL);
		memcpy(kernel[i], args[i], size);
        }

	/* Open the file. */
	result = vfs_open((char *)progname, O_RDONLY, 0, &v);
	if (result) {
		return result;
	}

	/* Save the old as and create a new one */
	as_deactivate();
	struct addrspace *old = curproc_getas();
	KASSERT(old != NULL);

	/* Create a new address space. */
	as = as_create();
	if (as == NULL) {
		vfs_close(v);
		return ENOMEM;
	}

	/* Switch to it and activate it. */
	curproc_setas(as);
	as_activate();

	/* Load the executable. */
	result = load_elf(v, &entrypoint);
	if (result) {
		/* p_addrspace will go away when curproc is destroyed */
		vfs_close(v);
		return result;
	}

	/* Done with the file now. */
	vfs_close(v);

	/* Define the user stack in the address space */
	result = as_define_stack(as, &stackptr);
	if (result) {
		/* p_addrspace will go away when curproc is destroyed */
		return result;
	}

	vaddr_t user_stack[n+1];

	stackptr = ROUNDUP(stackptr, 8);
	user_stack[n] = stackptr;

	for (int i = n - 1; i >= 0; --i) {
		int size = ROUNDUP(strlen(kernel[i]) + 1, 4);
		stackptr -= size;
		copyoutstr(kernel[i], (userptr_t)stackptr, (size_t)size, NULL);
		user_stack[i] = stackptr;
	}

	stackptr = stackptr - sizeof(vaddr_t);
        copyout((void *)NULL, (userptr_t)stackptr, (size_t)4);

	for (int i = n - 1; i >= 0; --i) {
		size_t size = sizeof(vaddr_t);
		stackptr -= size;
		copyout(&user_stack[i], (userptr_t)stackptr, size);
        }

	as_destroy(old);

	/* Warp to user mode. */
	enter_new_process(n /*argc*/, (userptr_t)stackptr /*userspace addr of argv*/,
			  stackptr, entrypoint);
	
	/* enter_new_process does not return. */
	panic("enter_new_process returned\n");
	return EINVAL;
}


  /* this implementation of sys__exit does not do anything with the exit code */
  /* this needs to be fixed to get exit() and waitpid() working properly */

void sys__exit(int exitcode) {
  struct addrspace *as;
  struct proc *p = curproc;

  DEBUG(DB_SYSCALL,"Syscall: _exit(%d)\n",exitcode);

  KASSERT(curproc->p_addrspace != NULL);
  as_deactivate();
  /*
   * clear p_addrspace before calling as_destroy. Otherwise if
   * as_destroy sleeps (which is quite possible) when we
   * come back we'll be calling as_activate on a
   * half-destroyed address space. This tends to be
   * messily fatal.
   */
  as = curproc_setas(NULL);
  as_destroy(as);

  /* detach this thread from its process */
  /* note: curproc cannot be used after this call */
  proc_remthread(curthread);

#if OPT_A2
  if (p->parent != NULL) {
	  p->exit_code = exitcode;
	  p->done = true;
	  lock_acquire(p->lk);
	  cv_signal(p->finished, p->lk);
	  lock_release(p->lk);
  }

#else
  /* for now, just include this to keep the compiler from complaining about
     an unused variable */
  (void)exitcode;
#endif

  /* if this is the last user process in the system, proc_destroy()
     will wake up the kernel menu thread */
  if (p->parent == NULL) proc_destroy(p);

  thread_exit();
  /* thread_exit() does not return, so we should never get here */
  panic("return from thread_exit in sys_exit\n");
}


/* stub handler for getpid() system call                */
int
sys_getpid(pid_t *retval)
{
  /* for now, this is just a stub that always returns a PID of 1 */
  /* you need to fix this to make it work properly */
#if OPT_A2
  *retval = curproc->pid;
#else
  *retval = 1;
#endif
  return(0);
}

/* stub handler for waitpid() system call                */

int
sys_waitpid(pid_t pid,
	    userptr_t status,
	    int options,
	    pid_t *retval)
{
  int exitstatus;
  int result;

  /* this is just a stub implementation that always reports an
     exit status of 0, regardless of the actual exit status of
     the specified process.   
     In fact, this will return 0 even if the specified process
     is still running, and even if it never existed in the first place.

     Fix this!
  */

  if (options != 0) {
    return(EINVAL);
  }

#if OPT_A2
  KASSERT(curproc != NULL);

  bool exists = false;
  for (unsigned int i = 0; i < array_num(curproc->children); ++i) {
  	struct proc *child = array_get(curproc->children, i);
    	if (child->pid == pid) {
  		lock_acquire(child->lk);
  		exists = true;
		while (child->done == false) {
    			cv_wait(child->finished, child->lk);
  		}
  		exitstatus = _MKWAIT_EXIT(child->exit_code);
  		lock_release(child->lk);  	
	}
  }

  if (exists == false) return (ECHILD);
#else
  /* for now, just pretend the exitstatus is 0 */
  exitstatus = 0;
#endif
  result =  copyout((void *)&exitstatus, status, sizeof(int));
  if (result) {
        return result;
  }

  *retval = pid;

  return(0);
}


int sys_fork(struct trapframe *tf, pid_t *retval)  {
  KASSERT(curproc != NULL);
  
  struct proc *child = proc_create_runprogram(curproc->p_name);
  if (child == NULL) return (ENOMEM);

  child->parent = curproc;

  spinlock_acquire(&child->p_lock);
  int res = as_copy(curproc_getas(), &(child->p_addrspace));
  spinlock_release(&child->p_lock);

  if (res != 0) {
        proc_destroy(child);
        return (ENOMEM);
  }

  spinlock_acquire(&curproc->p_lock);
  array_init(child->children);
  array_add(curproc->children, child, NULL);
  spinlock_release(&curproc->p_lock);

  struct trapframe *newtf = kmalloc(sizeof(struct trapframe));
  if (newtf == NULL) {
        proc_destroy(child);
        return (ENOMEM);
  }
  
  memcpy(newtf, tf, sizeof(struct trapframe));
  res = thread_fork("child", child, (void *)&enter_forked_process, newtf, 10);
  if (res) {
        kfree(newtf);
        proc_destroy(child);
        return(res);
  }

  *retval = child->pid;

  return 0;
}
