// implement fork from user space

#include <inc/string.h>
#include <inc/lib.h>

// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW		0x800

//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//
static void
pgfault(struct UTrapframe *utf)
{
	void *addr = (void *) utf->utf_fault_va;
	uint32_t err = utf->utf_err;
	int r;

	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
	// Hint:
	//   Use the read-only page table mappings at uvpt
	//   (see <inc/memlayout.h>).

	// LAB 4: Your code here.
	if((err & FEC_WR) == 0)
		panic("the page fault access is not to a writable page.\n");
	if(uvpt[PGNUM(addr)] & PTE_COW)
		panic("the page fault access is not to a copy-on-write page.\n");


	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.

	// LAB 4: Your code here.
	//panic("pgfault not implemented");
	envid_t senvid = sys_getenvid();
	if (sys_page_alloc(senvid, PFTEMP, PTE_U | PTE_P | PTE_W) < 0)
        panic("Can't allocate temp page.\n");
    memmove(PFTEMP, ROUNDDOWN(addr, PGSIZE), PGSIZE);
    sys_page_map(senvid, PFTEMP, senvid, (void*)ROUNDDOWN(addr, PGSIZE), PTE_U | PTE_P | PTE_W);
    sys_page_unmap(senvid, PFTEMP);
}

//
// Map our virtual page pn (address pn*PGSIZE) into the target envid
// at the same virtual address.  If the page is writable or copy-on-write,
// the new mapping must be created copy-on-write, and then our mapping must be
// marked copy-on-write as well.  (Exercise: Why do we need to mark ours
// copy-on-write again if it was already copy-on-write at the beginning of
// this function?)
//
// Returns: 0 on success, < 0 on error.
// It is also OK to panic on error.
//
static int
duppage(envid_t envid, unsigned pn)
{
	int r;

	// LAB 4: Your code here.
	//panic("duppage not implemented");
	envid_t senvid = sys_getenvid();
	pte_t pte = uvpt[pn];
	int perm = PTE_U | PTE_P;
	if(pte & PTE_COW || pte & PTE_W)
		perm |= PTE_COW;
	if((r = sys_page_map(senvid, (void*)(pn * PGSIZE), envid, (void*) (pn * PGSIZE), perm)) < 0)
		return r;
	if((perm & PTE_COW) && ((r = sys_page_map(senvid, (void*) (pn * PGSIZE), senvid, (void*) (pn * PGSIZE), perm)) < 0))
		return r;
	return 0;
}

//
// User-level fork with copy-on-write.
// Set up our page fault handler appropriately.
// Create a child.
// Copy our address space and page fault handler setup to the child.
// Then mark the child as runnable and return.
//
// Returns: child's envid to the parent, 0 to the child, < 0 on error.
// It is also OK to panic on error.
//
// Hint:
//   Use uvpd, uvpt, and duppage.
//   Remember to fix "thisenv" in the child process.
//   Neither user exception stack should ever be marked copy-on-write,
//   so you must allocate a new page for the child's user exception stack.
//
envid_t
fork(void)
{
	// LAB 4: Your code here.
	//panic("fork not implemented");
	envid_t senvid = sys_getenvid();
	envid_t cenvid = sys_exofork();
	extern void _pgfault_upcall(void);
	if(cenvid < 0)
		panic("child creation failed");
	if(cenvid == 0){
		thisenv = &envs[ENVX(sys_getenvid())];
		return cenvid;
	}

	for(int i = PDX(UTEXT); i < PDX(UXSTACKTOP); i++) {
		pde_t pde = uvpd[i];
		if(pde & PTE_P){
			for(int j = 0; j < NPTENTRIES ; j++){
				int pn = PGNUM(PGADDR(i, j, 0));
				if(pn == PGNUM(UXSTACKTOP - PGSIZE))
					continue;
				pte_t pte = uvpt[pn];
				if((pte & PTE_P) && ((pte & PTE_W) || (pte & PTE_COW))){
					if(duppage(cenvid, pn) < 0)
						return -1;
				}

			}
		}
	}
	set_pgfault_handler(pgfault);
	int errcode;
	if((errcode = sys_page_alloc(cenvid, (void *)(UXSTACKTOP - PGSIZE), PTE_P | PTE_W | PTE_U)) < 0)
		return errcode;
	//if((errcode = sys_page_map(cenvid, (void *)(UXSTACKTOP - PGSIZE), senvid, PFTEMP, PTE_P | PTE_W | PTE_U)) < 0)
	//	return errcode;
	//memmove((void *)(UXSTACKTOP - PGSIZE), PFTEMP, PGSIZE);
	//if((errcode = sys_page_unmap(senvid, PFTEMP)) < 0)
	//	return errcode;
	if((errcode = sys_env_set_pgfault_upcall(cenvid, (void*)_pgfault_upcall)) < 0)
        return errcode;
    if((errcode = sys_env_set_status(cenvid, ENV_RUNNABLE)) < 0){
        return errcode;
    }
	return cenvid;
}

// Challenge!
int
sfork(void)
{
	panic("sfork not implemented");
	return -E_INVAL;
}
