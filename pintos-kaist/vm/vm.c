/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include <stdio.h>
#include "threads/vaddr.h"



/* Initializes the virtual memory subsystem by invoking each subsystem's
 * intialize codes. */
uint64_t page_hash(const struct hash_elem *, void *aux);
bool page_less(const struct hash_elem *, const struct hash_elem *, void *aux);

void page_destory(struct hash_elem *e, void *aux);
void
vm_init (void) {
	vm_anon_init ();
	vm_file_init ();
#ifdef EFILESYS  /* For project 4 */
	pagecache_init ();
#endif
	register_inspect_intr ();
	/* DO NOT MODIFY UPPER LINES. */
	/* TODO: Your code goes here. */
}

/* Get the type of the page. This function is useful if you want to know the
 * type of the page after it will be initialized.
 * This function is fully implemented now. */
enum vm_type
page_get_type (struct page *page) {
	int ty = VM_TYPE (page->operations->type);
	switch (ty) {
		case VM_UNINIT:
			return VM_TYPE (page->uninit.type);
		default:
			return ty;
	}
}

/* Helpers */
static struct frame *vm_get_victim (void);
static bool vm_do_claim_page (struct page *page);
static struct frame *vm_evict_frame (void);

/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`. */
bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable,
		vm_initializer *init, void *aux) {
	ASSERT (VM_TYPE(type) != VM_UNINIT);
	struct supplemental_page_table *spt = &thread_current ()->spt;
	/* Check wheter the upage is already occupied or not. */
	if (spt_find_page (spt, upage) == NULL) {
		/* TODO: Create the page, fetch the initialier according to the VM type,
		 * TODO: and then create "uninit" page struct by calling uninit_new. You
		 * TODO: should modify the field after calling the uninit_new. */
		struct page *new_page = malloc(sizeof(struct page));
		bool(*initializer)(struct page *, enum vm_type, void *)=NULL;
		
		if (new_page==NULL) return false;

		switch(VM_TYPE(type)){
			case VM_ANON:
				initializer = anon_initializer;
				break;
			case VM_FILE:
				initializer = file_backed_initializer;
				break;
			default:
				free(new_page);
				return false;
		}
		/* TODO: Insert the page into the spt. */
		uninit_new(new_page,upage,init,type,aux,initializer);
		new_page->writable=writable;
		if(!spt_insert_page(spt,new_page)){
			//free(new_page);
			return false;
		}
		return true;
	}
err:
	//printf("페이지가 이미 있다.");
	return false;
}

/* Find VA from spt and return page. On error, return NULL. */
struct page *
spt_find_page (struct supplemental_page_table *spt UNUSED, void *va UNUSED) {
	//struct page *page = NULL;
	/* TODO: Fill this function. */
	struct page p;
	struct hash_elem *e;
	p.va=va;
	e = hash_find(&spt->spt, &p.hash_elem);

	return e!=NULL ? hash_entry(e,struct page,hash_elem):NULL;
}

/* Insert PAGE into spt with validation. */
bool
spt_insert_page (struct supplemental_page_table *spt UNUSED,
		struct page *page UNUSED) {
	int succ = false;
	/* TODO: Fill this function. */
	lock_acquire(&spt->spt_lock);
	struct hash_elem *p = hash_insert(&spt->spt,&page->hash_elem);
	lock_release(&spt->spt_lock);
	if(p==NULL){
		succ=true;
	}

	return succ;
}

void
spt_remove_page (struct supplemental_page_table *spt, struct page *page) {
	lock_acquire(&spt->spt_lock);
	struct hash_elem *p=hash_delete(&spt->spt,&page->hash_elem);
	if(p !=NULL){
		vm_dealloc_page (page);
	}
	lock_release(&spt->spt_lock);

	//return true;?
}

/* Get the struct frame, that will be evicted. */
static struct frame *
vm_get_victim (void) {
	struct frame *victim = malloc(sizeof(struct frame));
	 /* TODO: The policy for eviction is up to you. */

	return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *
vm_evict_frame (void) {
	struct frame *victim UNUSED = vm_get_victim ();
	/* TODO: swap out the victim and return the evicted frame. */

	return NULL;
}

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/
static struct frame *
vm_get_frame(void)
{
	struct frame *frame = malloc(sizeof(struct frame));
	/* TODO: Fill this function. */
	frame->page = NULL;
	frame->kva = palloc_get_page(PAL_USER);

	if (frame->kva == NULL)
	{
		// 추후 프레임 확보 알고리즘 구현
		// frame = vm_evict_frame();
		// 일단 지금은 return false로 처리
		printf("메모리 확보 실패\n");
		return false;
	}
	ASSERT(frame != NULL);
	ASSERT(frame->page == NULL);
	return frame;
}

/* Growing the stack. */
static void
vm_stack_growth (void *addr UNUSED) {
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp (struct page *page UNUSED) {
}

/* Return true on success */
bool
vm_try_handle_fault (struct intr_frame *f UNUSED, void *addr UNUSED,
		bool user UNUSED, bool write UNUSED, bool not_present UNUSED) {
	struct supplemental_page_table *spt UNUSED = &thread_current ()->spt;
	struct page *page = spt_find_page(spt,addr);//주소 예약 여부 확인
	/* TODO: Validate the fault */
	if(!not_present) return false;
	if(addr == NULL || is_kernel_vaddr(addr)) return false;//접근 유효성 체크
	/* TODO: Your code goes here */
	
	if (page == NULL)
	{
		// printf("페이지 예약정보 없음\n");
		sys_exit(-1);
		return false;
	}

	

	return vm_do_claim_page (page);
}

/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
void
vm_dealloc_page (struct page *page) {
	destroy (page);
	free (page);
}

/* Claim the page that allocate on VA. */
bool
vm_claim_page (void *va UNUSED) {//va에 접근해서 page fault가 발생했을 때 spt를 확인, 해당 주소의 page가 있으면 그 페이지를 물리 메모리에 올림.
	struct page *page = spt_find_page(&thread_current()->spt,va);
	/* TODO: Fill this function */
	if(page == NULL){
		return false;
	}

	return vm_do_claim_page (page);
}

/* Claim the PAGE and set up the mmu. */
static bool
vm_do_claim_page (struct page *page) {
	struct frame *frame = vm_get_frame ();
	if(frame == NULL){
		return false;
	}

	/* Set links */
	frame->page = page;
	page->frame = frame;

	/* TODO: Insert page table entry to map page's VA to frame's PA. */
	if(!swap_in(page,frame->kva)){
		//printf("파일 읽기 실패\n");
		return false;}
	if(!pml4_set_page(thread_current()->pml4,page->va,frame->kva,page->writable)){
		//printf("pml4 매핑 실패");
		return false;}


	//return swap_in (page, frame->kva);
	return true;
}

/* Initialize new supplemental page table */
void
supplemental_page_table_init (struct supplemental_page_table *spt UNUSED) {
	hash_init(&spt->spt,page_hash,page_less,NULL);
	lock_init(&spt->spt_lock);
}

/* Copy supplemental page table from src to dst */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst UNUSED,
		struct supplemental_page_table *src UNUSED) {
}

/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill (struct supplemental_page_table *spt UNUSED) {
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
	
	hash_destroy(&spt->spt,page_destory);
}
uint64_t page_hash(const struct hash_elem *p_, void *aux UNUSED){
	const struct page *p = hash_entry(p_,struct page, hash_elem);
	return hash_bytes(&p->va,sizeof p->va);//왜?
}
bool page_less(const struct hash_elem *a_,const struct hash_elem *b_,void *aux UNUSED){
	const struct page *a=hash_entry(a_,struct page, hash_elem);
	const struct page *b=hash_entry(b_,struct page, hash_elem);
	return a->va < b->va;
}
void page_destory(struct hash_elem *e, void *aux){
	struct page *page = hash_entry(e,struct page, hash_elem);
	destroy(page);
	free(page);
}