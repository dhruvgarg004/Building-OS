#include <types.h>
#include <mmap.h>
#include <fork.h>
#include <v2p.h>
#include <page.h>

/* 
 * You may define macros and other helper functions here
 * You must not declare and use any static/global paddriables 
 * */

void flush(){
    asm volatile("mov %cr3, %rax");
    asm volatile("mov %rax, %cr3");
    return;
}

long vm_area_unmap_helper(struct exec_context *current, u64 addr, int length){
    struct vm_area* curr=current->vm_area;
    struct vm_area* prev=NULL;
    struct vm_area* last=curr;
    u64 paddr;
    
    if(length%4096){
        length += 4096 - (length%4096);
    }
    while(curr!=NULL && curr->vm_end <= addr){
        prev= curr;
        curr=curr->vm_next;
    }
    if(curr == NULL){
        return 0;
    }
    last=curr;

    if((addr < curr->vm_start && addr+length > curr->vm_start) || (addr == curr->vm_start)){
        while(last!= NULL && addr+length > last->vm_end){
            stats->num_vm_area--;
            last=last->vm_next;
        }
        if(last == NULL){
            prev->vm_next = NULL;
        }
        if(addr + length == last->vm_end){
            stats->num_vm_area--;
            prev->vm_next=last->vm_next;
        }
        else{
            if(addr + length <= last->vm_start){
                prev->vm_next=last;
            }
            else{ 
            last->vm_start = addr+length;
            prev->vm_next = last;
            }
        }
        return 0;
    }
    else{
        if(addr + length == curr->vm_end){ 
            curr->vm_end=addr;
            return 0;
        }
        else if(addr + length < curr->vm_end){
            stats->num_vm_area++;
            struct vm_area* new_node=os_alloc(sizeof(struct vm_area));
            new_node->access_flags=curr->access_flags;
            new_node->vm_start=addr+length;
            new_node->vm_end=curr->vm_end;
            new_node->vm_next=curr->vm_next;
            curr->vm_end=addr;
            curr->vm_next=new_node;
            return 0;
        }
        else{
            struct vm_area* last = curr;
            while(last != NULL && last->vm_end < addr+length){
                if(last->vm_start >= addr){
                    stats->num_vm_area--;
                }
                last = last->vm_next;
            }
            if(last==NULL){
                curr->vm_end = addr;
                curr->vm_next=NULL;
            }
            else if(addr + length == last->vm_end){
                stats->num_vm_area--;
                curr->vm_end = addr;
                curr->vm_next=last->vm_next;
            }
            else{
                if(last->vm_start < addr+length){
                    last->vm_start = addr + length;
                }
                curr->vm_end = addr;
                curr->vm_next = last;
            }
            return 0;
        }
    }
    return -EINVAL;
}

int travel_page_unmap(struct exec_context * current,u64 addr){
    u64 *vp;
    vp=osmap(current->pgd);
    u64  offset=(addr >> 39) & 511;
    u64 * pgd_t=(u64*)(offset+(u64*)vp);
    u64 pud= *(pgd_t);
    u64 pud_pfn;
    if(pud&1){
        pud_pfn=pud>>12;
    }
    else{
        return 1;
    }
   
    vp=osmap(pud_pfn);
    offset=(addr >> 30) & 511;
    u64 * pud_t=(u64*)(offset+(u64*)vp);;
    u64 pmd= *(pud_t);
    u64 pmd_pfn;
    if(pmd&1){
        pmd_pfn=pmd>>12;
    }
    else{
        return 1;
    }

    vp=osmap(pmd_pfn);
    offset=(addr >> 21) & 511;
    u64 * pmd_t=(u64*)(offset+(u64*)vp);
    u64 pte= *(pmd_t);
    u64 pte_pfn;
    if(pte&1){
        pte_pfn=pte>>12;
    }
    else{
        return 1;
    }

    vp=osmap(pte_pfn);
    offset=(addr >> 12) & 511;
    u64 * pte_t=(u64*)(offset+(u64*)vp);
    u64 phyf= *(pte_t);
    u64 phyf_pfn;
    if(phyf&1){
        if(get_pfn_refcount(phyf>>12)==1){
            put_pfn(phyf>>12);
            os_pfn_free(USER_REG,phyf>>12);
            *(u64 *)pte_t=0;
        }
        else{
            put_pfn(phyf>>12);
            *(u64 *)pte_t=0;
        }
        flush();   
    }
    return 1;
}

int travel_page_protect(struct exec_context * current,u64 addr, int prot){
    u64 *vp;
    vp=osmap(current->pgd);
    u64 offset=(addr >> 39) & 511;
    u64 * pgd_t=(u64*)(offset+(u64*)vp);
    u64 pud= *(pgd_t);
    u64 pud_pfn;
    if(pud&1){
        pud_pfn=pud>>12;
    }
    else{
        return 1;
    }
   
    vp=osmap(pud_pfn);
    offset=(addr >> 30) & 511;
    u64 * pud_t=(u64*)(offset+(u64*)vp);
    u64 pmd= *(pud_t);
    u64 pmd_pfn;
    if(pmd&1){
        pmd_pfn=pmd>>12;
    }
    else{
        return 1;
    }

    vp=osmap(pmd_pfn);
    offset=(addr >> 21) & 511;
    u64 * pmd_t=(u64*)(offset+(u64*)vp);
    u64 pte= *(pmd_t);
    u64 pte_pfn;
    if(pte&1){
        pte_pfn=pte>>12;
    }
    else{
        return 1;
    }

    vp=osmap(pte_pfn);
    offset=(addr >> 12) & 511;
    u64 * pte_t=(u64*)(offset+(u64*)vp);
    u64 phyf= *(pte_t);
    u64 phyf_pfn;
    if(phyf&1){
        if(get_pfn_refcount(phyf>>12)>1){
            return 1;
        }
        if(prot==PROT_READ){
            if(((*pte_t & 8)>>3)==1){
                *(u64 *)pte_t=*(u64 *)pte_t-8;
            } 
        }
        else{               
            *(u64 *)pte_t=*(u64 *)pte_t|8;
        }
        flush();
    }
    return 1;

}

void travel_page_table(struct exec_context *ctx, struct exec_context *new_ctx,u64 addr){
    u64* first,*second;
    first=osmap(ctx->pgd);  
    second=osmap(new_ctx->pgd);
    u64 offset = (addr >> 39) & 511;
    u64 * pgd_t1=(u64*)(offset+(u64*)first);
    u64 pud1= *(pgd_t1);
    u64 * pgd_t2=(u64*)(offset+(u64*)second);
    u64 pud2= *(pgd_t2);
    u64 pud_pfn1,pud_pfn2;
    if((pud1 & 1)==0){
        return;
        }
    else if((pud2 & 1)==1){
        pud_pfn1=pud1>>12;
        pud_pfn2=pud2>>12;
    }
    else{
        pud_pfn1=pud1>>12;
        pud_pfn2=os_pfn_alloc(OS_PT_REG);
        *(pgd_t2) = (pud_pfn2<<12)|25;
    }
    first=osmap(pud_pfn1);  
    second=osmap(pud_pfn2);
    offset = (addr >> 30) & 511;
    u64 * pud_t1=(u64*)(offset+(u64*)first);
    u64 pmd1= *(pud_t1);
    u64 * pud_t2=(u64*)(offset+(u64*)second);
    u64 pmd2= *(pud_t2);
    u64 pmd_pfn1,pmd_pfn2;
    if((pmd1 & 1)==0){
        return;
    }
    else if((pmd2 & 1)==1){
        pmd_pfn1=pmd1>>12;
        pmd_pfn2=pmd2>>12;
    }
    else{
        pmd_pfn1=pmd1>>12;
        pmd_pfn2=os_pfn_alloc(OS_PT_REG); 
        *(pud_t2) = (pmd_pfn2<<12)|25;
    }
    first=osmap(pmd_pfn1);  
    second=osmap(pmd_pfn2);
    offset = (addr >> 21) & 511;
    u64 * pmd_t1=(u64*)(offset+(u64*)first);
    u64 pte1= *(pmd_t1);
    u64 * pmd_t2=(u64*)(offset+(u64*)second);
    u64 pte2= *(pmd_t2);
    u64 pte_pfn1,pte_pfn2;
    if((pte1 & 1)==0){
        return;
    }
    else if((pte2 & 1)==1){
        pte_pfn1=pte1>>12;
        pte_pfn2=pte2>>12;
    }
    else{
        pte_pfn1=pte1>>12;
        pte_pfn2=os_pfn_alloc(OS_PT_REG);
        *(pmd_t2) = (pte_pfn2<<12)|25;
    }
    first=osmap(pte_pfn1);  
    second=osmap(pte_pfn2);
    offset = (addr >> 12) & 511;
    u64 * pte_t1=(u64*)(offset+(u64*)first);
    u64 phy1= *(pte_t1);
    u64 * pte_t2=(u64*)(offset+(u64*)second);
    u64 phy2= *(pte_t2);
    u64 phy_pfn1,phy_pfn2;
    if((phy1 & 1)==0){
        return;
    }            
    else{
        get_pfn((*(pte_t1))>>12);
        *(pte_t1)=*(pte_t1) & 0xFFFFFFFFFFFFFFF7;
        *(pte_t2)=*(pte_t1);
    }
    return;
}

u64 min(u64 a,u64 b){
    if(a<b) return a;
    return b;
}

u64 max(u64 a,u64 b){
    if(a<b) return b;
    return a;
}

/**
 * mprotect System call Implementation.
 */

long vm_area_mprotect(struct exec_context *current, u64 addr, int length, int prot){
    struct vm_area* curr=current->vm_area;
    struct vm_area* prev=NULL;
    
    if(length%4096){
        length += 4096 - (length%4096);
    }

    while(curr!= NULL && curr->vm_end <= addr){   
        prev=curr;
        curr=curr->vm_next;
    }
    if(curr == NULL){
        return 0;
    }
    u64 curraddr=addr;
    while(curr!= NULL && curr->vm_start < addr + length){
        if(curr->access_flags==prot){
            curr=curr->vm_next;
            continue;
        }
        for(u64 paddr=max(curr->vm_start,addr);paddr+4096<=min(addr+length,curr->vm_end);paddr=paddr+4096){
            travel_page_protect(current, paddr,prot);
        }
        curraddr=max(curr->vm_start,addr);
        u64 length_now=min(addr+length,curr->vm_end) - max(curr->vm_start,addr);
        if(vm_area_unmap_helper(current,curraddr,length_now) < 0){
            return -1;
        }
        if(vm_area_map(current,curraddr,length_now,prot,MAP_FIXED) < 0){
            return -1;
        }
        curr=curr->vm_next;
    }

    return 0;
}

/**
 * mmap system call implementation.
 */
long vm_area_map(struct exec_context *current, u64 addr, int length, int prot, int flags){
    if(prot != 1 && prot !=3) return -1;
    if(flags!= 0 && flags != MAP_FIXED) return -1;
    if(length < 0) return -1;
    if(length%4096){
        length += 4096 - (length%4096);
    }

    struct vm_area* curr = current->vm_area;
    if(curr == NULL){
        stats->num_vm_area++;
        struct vm_area* new_node = os_alloc(sizeof(struct vm_area));
        new_node->vm_start = MMAP_AREA_START;
        new_node->vm_end = MMAP_AREA_START + 4096;
        new_node->access_flags = 0x0;
        curr = new_node;
        current -> vm_area = new_node; 
    }
    struct vm_area* add_node = os_alloc(sizeof(struct vm_area));
    struct vm_area* prev = NULL;
    add_node->access_flags = prot;
    add_node->vm_next = NULL;
    u64 ans;
    if( addr == 0){
        if(prev == NULL && curr->vm_next == NULL){
            stats->num_vm_area++;
            add_node->vm_start=curr->vm_end;
            add_node->vm_end = curr->vm_end+length;
            curr->vm_next=add_node;
            return add_node->vm_start;
        }
        else if(prev == NULL && curr->vm_next!=NULL){
            prev=curr;
            curr=curr->vm_next;
        }
        while(curr != NULL){ 
            ans=prev->vm_end;
            if(curr->vm_start - prev->vm_end == length){
                if(curr->access_flags == prev->access_flags && curr->access_flags == add_node->access_flags){
                    prev->vm_end = curr->vm_end;
                    prev->vm_next = curr->vm_next;
                    stats->num_vm_area--;
                }
                else if(curr->access_flags == add_node->access_flags){
                    curr->vm_start -= length;
                }
                else if(add_node->access_flags == prev->access_flags){
                    prev->vm_end += length;
                }
                else{
                    stats->num_vm_area++;
                    prev->vm_next = add_node;
                    add_node->vm_next = curr;
                    add_node->vm_start = prev->vm_end;
                    add_node->vm_end = curr->vm_start;
                }
                return ans;
            }
            else if(curr->vm_start - prev->vm_end > length){
                if(add_node->access_flags == prev->access_flags){
                    prev->vm_end += length;
                }
                else{
                    stats->num_vm_area++;
                    prev->vm_next = add_node;
                    add_node->vm_next = curr;
                    add_node->vm_start = prev->vm_end;
                    add_node->vm_end = add_node->vm_start + length;
                }
                return ans;
            }
            prev = curr;
            curr = curr->vm_next;
        }
        curr=prev;
        ans = curr->vm_end;
        if(curr->access_flags == add_node->access_flags){
            curr->vm_end += length;
            return ans;
        }
        else{
            stats->num_vm_area++;
            curr->vm_next = add_node;
            add_node->vm_start = curr->vm_end;
            add_node->vm_end = add_node->vm_start+length;
            return ans;
        }
    }

    else{
        while(curr != NULL && curr->vm_end <= addr ){
            prev=curr; 
            curr= curr->vm_next;
        } 
        if(curr== NULL){
            if(prev->vm_end != addr){
                stats->num_vm_area++;
                prev->vm_next = add_node;
                add_node-> vm_start = addr;
                add_node->vm_end=addr+length;
            }
            else if(prev->vm_end == addr && prev->access_flags == add_node->access_flags){
                prev->vm_end += length;
            }
            else{
                stats->num_vm_area++;
                prev-> vm_next = add_node;
                add_node-> vm_start = addr;
                add_node->vm_end=addr+length;
            }
            return addr;
        }
        else{ 
            if(addr >= curr->vm_start || (addr < curr->vm_start && (addr+length > curr->vm_start))){
                if(flags == MAP_FIXED){
                    return -1;
                }
                else{ 
                    return vm_area_map(current,0,length,prot,flags);
                }
            }
            else{
                if(addr+length < curr->vm_start && prev->vm_end == addr){
                    if(prev->access_flags == add_node->access_flags){
                        prev->vm_end+= length;
                    }
                    else{
                        stats->num_vm_area++;
                        prev->vm_next = add_node;
                        add_node->vm_next= curr;
                        add_node->vm_start=addr;
                        add_node->vm_end=addr+length;
                    }
                }
                else if(addr+length < curr->vm_start){
                    stats->num_vm_area++;
                    prev->vm_next = add_node;
                    add_node->vm_next= curr;
                    add_node->vm_start=addr;
                    add_node->vm_end=addr+length;
                }
                else if(addr+length == curr->vm_start && prev->vm_end == addr){
                    if(curr->access_flags == prev->access_flags && curr->access_flags == add_node->access_flags){
                        prev->vm_end = curr->vm_end;
                        prev->vm_next = curr->vm_next;return ans;
                        stats->num_vm_area--;
                    }
                    else if(add_node->access_flags == prev->access_flags){
                        prev->vm_end += length;
                    }
                    else if(curr->access_flags == add_node->access_flags){
                        curr->vm_start -= length;
                    }
                    else{
                        stats->num_vm_area++;
                        prev->vm_next = add_node; 
                        add_node->vm_next = curr; 
                        add_node->vm_start = prev->vm_end;
                        add_node->vm_end = curr->vm_start; 
                    }
                }
                else if(addr+length == curr->vm_start){
                    if(curr->access_flags == add_node->access_flags){
                        curr->vm_start -= length;
                    }
                    else{
                        stats->num_vm_area++;
                        prev->vm_next = add_node;
                        add_node->vm_next= curr;
                        add_node->vm_start=addr;
                        add_node->vm_end=addr+length;
                    }
                }
                return addr;
            }
        }
    }
    return -EINVAL;
}

/**
 * munmap system call implemenations
 */

long vm_area_unmap(struct exec_context *current, u64 addr, int length){
    struct vm_area* curr=current->vm_area;
    struct vm_area* prev=NULL;
    struct vm_area* last=curr;
    u64 paddr;
    
    
    if(length%4096){
        length += 4096 - (length%4096);
    }

    while(curr!=NULL && curr->vm_end <= addr){
        prev= curr;
        curr=curr->vm_next;
    }
    if(curr == NULL){
        return 0;
    }
    last=curr;

    if((addr < curr->vm_start && addr+length > curr->vm_start) || (addr == curr->vm_start)){
        while(last!= NULL && addr+length > last->vm_end){
            for(paddr=last->vm_start;paddr+4096<=last->vm_end;paddr=paddr+4096){ 
                travel_page_unmap(current,paddr);
            }
            stats->num_vm_area--;
            last=last->vm_next;
        }
        if(last == NULL){
            prev->vm_next = NULL;
        }
        if(addr + length == last->vm_end){
            for(paddr=last->vm_start;paddr+4096<=last->vm_end;paddr=paddr+4096){ 
                travel_page_unmap(current,paddr);
            }
            stats->num_vm_area--;
            prev->vm_next=last->vm_next;
        }
        else{
            if(addr + length <= last->vm_start){
                prev->vm_next=last;
            }
            else{
                for(paddr=last->vm_start;paddr+4096<=addr+length;paddr=paddr+4096){ 
                travel_page_unmap(current,paddr);
            }
            last->vm_start = addr+length;
            prev->vm_next = last;
            }
        }
        return 0;
    }
    else{
        if(addr + length == curr->vm_end){
            for(paddr=addr;paddr+4096<=addr+length;paddr=paddr+4096){ 
                travel_page_unmap(current,paddr);
            }
            curr->vm_end=addr;
            return 0;
        }
        else if(addr + length < curr->vm_end){
            for(paddr=addr;paddr+4096<=addr+length;paddr=paddr+4096){
                travel_page_unmap(current,paddr);
            }
            stats->num_vm_area++;
            struct vm_area* new_node=os_alloc(sizeof(struct vm_area));
            new_node->access_flags=curr->access_flags;
            new_node->vm_start=addr+length;
            new_node->vm_end=curr->vm_end;
            new_node->vm_next=curr->vm_next;
            curr->vm_end=addr;
            curr->vm_next=new_node;
            return 0;
        }
        else{
            struct vm_area* last = curr;
            while(last != NULL && last->vm_end < addr+length){
                if(last->vm_start >= addr){
                    stats->num_vm_area--;
                }
                for(paddr=addr;paddr+4096<=last->vm_end;paddr=paddr+4096){ 
                    travel_page_unmap(current,paddr);
                }
                last = last->vm_next;
            }
            if(last==NULL){
                curr->vm_end = addr;
                curr->vm_next=NULL;
            }
            else if(addr + length == last->vm_end){
                for(paddr=last->vm_start;paddr+4096<=last->vm_end;paddr=paddr+4096){ 
                    travel_page_unmap(current,paddr);
                }
                stats->num_vm_area--;
                curr->vm_end = addr;
                curr->vm_next=last->vm_next;
            }
            else{
                if(last->vm_start < addr+length){
                    for(paddr=last->vm_start;paddr+4096<=addr+length;paddr=paddr+4096){ 
                        travel_page_unmap(current,paddr);
                    }
                    last->vm_start = addr + length;
                }
                curr->vm_end = addr;
                curr->vm_next = last;
            }
            return 0;
        }
    }
    return -EINVAL;
}

/**
 * Function will invoked whenever there is page fault for an address in the curr area region
 * created using mmap
 */
long vm_area_pagefault(struct exec_context *current, u64 addr, int error_code){
    struct vm_area * curr =current->vm_area;
    while(curr!=NULL){ 
        if(addr>= curr->vm_start && addr < curr->vm_end){
            break;
        }
        else{
            curr=curr->vm_next;
        }
    }
    if(curr == NULL || (error_code==0x7 && curr->access_flags==PROT_READ) ){
        return -1;
    }
    else if (error_code==0x7){
       return handle_cow_fault(current,addr,PROT_READ | PROT_WRITE); 
    }
    if(error_code==0x6 && curr->access_flags==PROT_READ){
        return -1;
    }
    else if(error_code==0x4 || error_code==0x6){
        u64 *vp;
        vp=osmap(current->pgd);
        u64 offset = (addr >> 39) & 511;
        u64 * pgd_t=(u64*)(offset+(u64*)vp);
        u64 pud= *(pgd_t);
        u64 pud_pfn;
        if(pud&1){
            pud_pfn=pud>>12;
        }
        else{
            pud_pfn=os_pfn_alloc(OS_PT_REG);
            u64 entry = pud_pfn<<12;
            entry=entry + 1 + 8 + 16;
            *(pgd_t)=entry;
        }
        vp=osmap(pud_pfn);
        offset = (addr >> 30) & 511;
        u64 * pud_t=(u64*)(offset+(u64*)vp);
        u64 pmd= *(pud_t);
        u64 pmd_pfn;
        if(pmd&1){
            pmd_pfn=pmd>>12;
        }
        else{
            pmd_pfn=os_pfn_alloc(OS_PT_REG);
            u64 entry = pmd_pfn<<12;
            entry=entry + 1 + 8 + 16;
            *(pud_t)=entry;
        }
        vp=osmap(pmd_pfn);
        offset = (addr >> 21) & 511;
        u64 * pmd_t=(u64*)(offset+(u64*)vp);
        u64 pte= *(pmd_t);
        u64 pte_pfn;
        if(pte&1){
            pte_pfn=pte>>12;
        }
        else{
            pte_pfn=os_pfn_alloc(OS_PT_REG);            
            u64 entry = pte_pfn<<12;
            entry=entry + 1 + 8 + 16;
            *(pmd_t)=entry;
        }
        vp=osmap(pte_pfn);
        offset = (addr >> 12) & 511;
        u64 * pte_t=(u64*)(offset+(u64*)vp);
        u64 phyf= *(pte_t);
        u64 phyf_pfn;
        int prot=curr->access_flags;
        if(phyf&1){
            phyf_pfn=phyf>>12;
        }
        else{
            phyf_pfn=os_pfn_alloc(USER_REG);
            u64 entry = (phyf_pfn<<12) + 1 + 16;
            if(prot== PROT_READ|PROT_WRITE ){
                entry =entry+ 8;
            }
            *(pte_t)=entry;
        }
        flush();
    }
    return 1;
    
}

/**
 * cfork system call implemenations
 * The parent returns the pid of child process. The return path of
 * the child process is handled separately through the calls at the 
 * end of this function (e.g., setup_child_context etc.)
 */

long do_cfork(){
    u32 pid;
    struct exec_context *new_ctx = get_new_ctx();
    struct exec_context *ctx = get_current_ctx();
     /* Do not modify above lines
     * 
     * */   
     /*--------------------- Your code [start]---------------*/
    u32 cpid=new_ctx->pid;
    pid=new_ctx->pid;
    memcpy(new_ctx,   ctx, sizeof(*ctx));
    new_ctx->pid=cpid;
    new_ctx->ppid=ctx->pid;
    struct vm_area * temp=ctx->vm_area,*head;
    struct vm_area * temp2=NULL;
    new_ctx->pgd=os_pfn_alloc(OS_PT_REG);
   
    while(temp!=NULL){
        struct vm_area* new_node=os_alloc(sizeof(struct vm_area));
        new_node->access_flags=temp->access_flags;
        new_node->vm_start=temp->vm_start;
        new_node->vm_end=temp->vm_end;
        new_node->vm_next=NULL;
        for(u64 addr =temp->vm_start;addr<temp->vm_end;addr=addr+4096){
            travel_page_table(ctx,new_ctx,addr);
        }
            
        if(temp2=NULL){ 
            temp2=new_node;
            new_ctx->vm_area=temp2;
        }
        else{
            temp2->vm_next==new_node;
            temp2=temp2->vm_next;
        }
        temp=temp->vm_next;
    }
    for(int i=0;i<4;i++){
        u64 sp,lim;
        sp=ctx->mms[i].start;
        if(i<3){
            lim=ctx->mms[i].next_free;
        }
        else{
            lim=ctx->mms[i].end;
        }
        for(u64 addr=sp;addr<lim;addr+=4096){
            travel_page_table(ctx,new_ctx,addr);
        }

    }

     /*--------------------- Your code [end] ----------------*/
    
     /*
     * The remaining part must not be changed
     */
    copy_os_pts(ctx->pgd, new_ctx->pgd);
    do_file_fork(new_ctx);
    setup_child_context(new_ctx);
    return pid;
}

/* Cow fault handling, for the entire user address space
 * For address belonging to memory segments (i.e., stack, data) 
 * it is called when there is a CoW violation in these areas. 
 *
 * For curr areas, your fault handler 'vm_area_pagefault'
 * should invoke this function
 * */

long handle_cow_fault(struct exec_context *current, u64 vaddr, int access_flags){        
        u64 *vp;      
        vp=osmap(current->pgd);
        u64 offset = (vaddr >> 39) & 511;;
        u64 * pgd_t=(u64*)(offset+(u64*)vp);
        u64 pud= *(pgd_t);
        u64 pud_pfn;
        if((pud & 1)==0){
           return -1;
        }
        else{
            pud_pfn=pud>>12;
        }
        vp=osmap(pud_pfn);
        offset = (vaddr >> 30) & 511;
        u64 * pud_t=(u64*)(offset+(u64*)vp);
        u64 pmd= *(pud_t);
        u64 pmd_pfn;
        if((pmd & 1)==0){
          return -1;
        }
        else{
            pmd_pfn=pmd>>12;
        }
        vp=osmap(pmd_pfn);
        offset = (vaddr >> 21) & 511;
        u64 * pmd_t=(u64*)(offset+(u64*)vp);
        u64 pte= *(pmd_t);
        u64 pte_pfn;
        if((pte & 1)==0){
            return -1;
        }
        else{
            pte_pfn=pte>>12;
        }
        vp=osmap(pte_pfn);
        offset = (vaddr >> 12) & 511;
        u64 * pte_t=(u64*)(offset+(u64*)vp);
        u64 phyf= *(pte_t);
        u64 phyf_pfn;
        if((phyf & 1)==0){
            return -1;
        }
        else{
            if(get_pfn_refcount(phyf>>12)==1){
                *(pte_t)=*(pte_t)+8;
            }
            else{
                phyf_pfn=os_pfn_alloc(USER_REG);
                memcpy(osmap(phyf_pfn),osmap(phyf>>12),4096);
                put_pfn(phyf>>12);
                u64 entry = phyf_pfn<<12;
                entry = entry + 1 + 8 + 16;
                *(pte_t)=entry;
            }
            flush();
            return 1;
            
        }
  
  return -1;
}

