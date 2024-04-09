#include<context.h>
#include<memory.h>
#include<lib.h>
#include<entry.h>
#include<file.h>
#include<tracer.h>


///////////////////////////////////////////////////////////////////////////
//// 		Start of Trace buffer functionality 		      /////
///////////////////////////////////////////////////////////////////////////


u32 min(u32 a, u32 b)
{
	if (a>b)
	return b;

	return a;
}

int is_valid_mem_range(unsigned long buff, u32 count, int access_bit) 
{
	struct exec_context* ctx = get_current_ctx();

	if( (buff >= ctx->mms[MM_SEG_CODE].start && buff < ctx->mms[MM_SEG_CODE].next_free && (access_bit & 1))
	|| (buff >= ctx->mms[MM_SEG_RODATA].start && buff<ctx->mms[MM_SEG_RODATA].next_free && (access_bit & 1))
    || (buff >= ctx->mms[MM_SEG_DATA].start && buff < ctx->mms[MM_SEG_DATA].next_free && (access_bit & 3))
	|| (buff >= ctx->mms[MM_SEG_STACK].start && buff < ctx->mms[MM_SEG_STACK].end && (access_bit & 3))
	){
		return 0;
	}

	struct vm_area* area = ctx->vm_area;
	while (area!=NULL){
		if (buff>=area->vm_start && buff < area->vm_end && (access_bit & area->access_flags))
		{
			return 0;
		}
		area = area->vm_next;
	}
	return 1;
}

long trace_buffer_close(struct file *filep)
{
	os_free(filep->fops,sizeof(struct fileops));
	os_page_free(USER_REG,filep->trace_buffer->buffer);
	os_free(filep->trace_buffer,sizeof(struct trace_buffer_info));
	os_page_free(USER_REG,filep);
	return 0;	
}


int trace_buffer_read(struct file *filep, char *buff, u32 count)
{
	if (is_valid_mem_range((unsigned long)buff,count,2)){
		return -EBADMEM;
	}

	struct trace_buffer_info* curr_buff = filep->trace_buffer;

	if(curr_buff->mode == O_WRITE) return -EINVAL;
	if(curr_buff->empty == 1) return 0;

	u32 wo = curr_buff->write_offset;
	u32 ro = curr_buff->read_offset;
	u32 bytes_read;
	int i;

	if(ro >= wo  && count > 4096 - ro){
		if(4096+wo-ro <= count) curr_buff->empty = 1;
		bytes_read = min(count,4096 - ro + wo);
		for (i=0;i<4096 - ro;i++){
			buff[i] = curr_buff->buffer[i+ro];
		}
		for (i = 0 ;i<bytes_read - (4096-ro);i++){
			buff[i + 4096 - ro] = curr_buff->buffer[i];
		}
		curr_buff->read_offset=i;
	}
	else if(ro >= wo && count <= 4096 - ro){
		bytes_read = min(count,4096 - ro);
		for (i=0;i<bytes_read;i++){
			buff[i] = curr_buff->buffer[i+ro];
		}
		curr_buff->read_offset = bytes_read + ro;
	}
	else{
		if(wo-ro <= count){
			curr_buff->empty = 1;
		}
		bytes_read = min(count, wo-ro);
		for (i=0;i<bytes_read;i++){
			buff[i] = curr_buff->buffer[i+ro];
		}
		curr_buff->read_offset =bytes_read + ro;
	}
	return bytes_read;
}

int trace_buffer_read_2(struct file *filep, char *buff, u32 count)
{
	
	struct trace_buffer_info* curr_buff = filep->trace_buffer;

	if(curr_buff->mode == O_WRITE) return -EINVAL;
	if(curr_buff->empty == 1) return 0;

	u32 wo = curr_buff->write_offset;
	u32 ro = curr_buff->read_offset;
	u32 bytes_read;
	int i;

	if(ro >= wo  && count > 4096 - ro){
		if(4096+wo-ro <= count) curr_buff->empty = 1;
		bytes_read = min(count,4096 - ro + wo);
		for (i=0;i<4096 - ro;i++){
			buff[i] = curr_buff->buffer[i+ro];
		}
		for (i = 0 ;i<bytes_read - (4096-ro);i++){
			buff[i + 4096 - ro] = curr_buff->buffer[i];
		}
		curr_buff->read_offset=i;
	}
	else if(ro >= wo && count <= 4096 - ro){
		bytes_read = min(count,4096 - ro);
		for (i=0;i<bytes_read;i++){
			buff[i] = curr_buff->buffer[i+ro];
		}
		curr_buff->read_offset = bytes_read + ro;
	}
	else{
		if(wo-ro <= count){
			curr_buff->empty = 1;
		}
		bytes_read = min(count, wo-ro);
		for (i=0;i<bytes_read;i++){
			buff[i] = curr_buff->buffer[i+ro];
		}
		curr_buff->read_offset =bytes_read + ro;
	}
	return bytes_read;
}


int trace_buffer_write(struct file *filep, char *buff, u32 count)
{

	if (is_valid_mem_range((unsigned long)buff,count,1)){
		return -EBADMEM;
	}
	
	int i;
	struct trace_buffer_info* curr_buff = filep->trace_buffer;
	if (curr_buff->mode==O_READ) return -EINVAL;
	if(count) curr_buff->empty =0;
	u32 ro = curr_buff->read_offset;
	u32 wo = curr_buff->write_offset;
	u32 bytes_write;

	if(ro <= wo && count > 4096-wo){
		bytes_write= min(count,4096 - wo + ro);	
		for (i = 0;i<4096-wo;i++){
			curr_buff->buffer[i + wo] = buff[i];
		}
		for (i=0;i<bytes_write - (4096-wo);i++){
			curr_buff->buffer[i] = buff[i + 4096 - wo];
		}
		curr_buff->write_offset = i;
	}
	else if(ro <= wo && count <= 4096-wo){
		bytes_write= min(count,4096 - wo);
		for (i = 0;i<bytes_write;i++){
			curr_buff->buffer[i + wo] = buff[i];
		}
		curr_buff->write_offset = bytes_write + wo;
	}
	else{
		bytes_write = min(count,ro-wo);
		for (i = 0;i<bytes_write;i++){
			curr_buff->buffer[i + wo] = buff[i];
		}
		curr_buff->write_offset = i + wo;
	}

	return bytes_write;
}

int trace_buffer_write_2(struct file *filep, char *buff, u32 count)
{

	int i;
	struct trace_buffer_info* curr_buff = filep->trace_buffer;
	if (curr_buff->mode==O_READ) return -EINVAL;
	if(count) curr_buff->empty =0;
	u32 ro = curr_buff->read_offset;
	u32 wo = curr_buff->write_offset;
	u32 bytes_write;

	if(ro <= wo && count > 4096-wo){
		bytes_write= min(count,4096 - wo + ro);	
		for (i = 0;i<4096-wo;i++){
			curr_buff->buffer[i + wo] = buff[i];
		}
		for (i=0;i<bytes_write - (4096-wo);i++){
			curr_buff->buffer[i] = buff[i + 4096 - wo];
		}
		curr_buff->write_offset = i;
	}
	else if(ro <= wo && count <= 4096-wo){
		bytes_write= min(count,4096 - wo);
		for (i = 0;i<bytes_write;i++){
			curr_buff->buffer[i + wo] = buff[i];
		}
		curr_buff->write_offset = bytes_write + wo;
	}
	else{
		bytes_write = min(count,ro-wo);
		for (i = 0;i<bytes_write;i++){
			curr_buff->buffer[i + wo] = buff[i];
		}
		curr_buff->write_offset = i + wo;
	}

	return bytes_write;
}

int sys_create_trace_buffer(struct exec_context *current, int mode)
{
	int fd;
	int i;

	if(mode!= O_RDWR && mode != O_READ && mode!=O_WRITE){
		return -EINVAL;
	}

	for (int i=0;i<MAX_OPEN_FILES;i++){
		if (current->files[i]==NULL){
			fd = i;
			break;
		}
	}
	if (i==MAX_OPEN_FILES) return -EINVAL;

	struct file* file_curr = (struct file*)os_page_alloc(USER_REG);
	current->files[fd]=file_curr;
	if (file_curr==NULL)return -ENOMEM;

	file_curr->type = 1; //Check it when done
	file_curr->mode = mode;
	file_curr->offp=0;
	file_curr->ref_count=1;
	file_curr->inode = NULL;

	struct fileops* file_op_curr = (struct fileops*)os_alloc(sizeof(struct fileops));
	if (file_op_curr==NULL){
		return -ENOMEM;
	}
	file_op_curr->close=&(trace_buffer_close);
	file_op_curr->read=&(trace_buffer_read);
	file_op_curr->write=&(trace_buffer_write);
	file_curr->fops = file_op_curr;

	struct trace_buffer_info* buff_curr = (struct trace_buffer_info*)os_alloc(sizeof(struct trace_buffer_info));
	if (buff_curr==NULL){
		return -ENOMEM;
	}
	
	buff_curr->buffer = (char*)os_page_alloc(USER_REG);
	buff_curr->empty=1;
	file_curr->trace_buffer = buff_curr;
	buff_curr->read_offset=0;
	buff_curr->write_offset=0;
	buff_curr->mode=mode;

	return fd;
}


///////////////////////////////////////////////////////////////////////////
//// 		Start of strace functionality 		      	      /////
///////////////////////////////////////////////////////////////////////////

int get_no_arguments(u64 syscall_num)
{
	u64 n=syscall_num;
	int arg=0;
	if (n == 2 || n == 10 || n == 11 || n == 13 || n == 15 || n == 20 || n == 21 || n == 22 || n == 38){
		arg=0;
	}
	else if (n == 1 || n == 6 || n == 7 || n == 12 || n == 14 || n == 19 || n == 27 || n == 29 || n==36){
		arg=1;
	}
	else if (n == 4 || n == 8 || n == 9 || n == 17 || n == 23 || n == 28 || n == 37 || n == 40){
		arg=2;
	}
	else if (n == 5 || n == 18 || n == 24 || n == 25 || n == 30 || n == 39 || n == 41){
		arg=3;
	}
	else if (n == 16 || n == 35){
		arg=4;
	}
	else{	
		return -1;
	}
	return arg;
}


int perform_tracing(u64 syscall_num, u64 param1, u64 param2, u64 param3, u64 param4)
{
	if(syscall_num>=37 && syscall_num<=40 ){
		return 0;
	}
	struct exec_context* ctx = get_current_ctx();
	struct strace_head* head_ls=ctx->st_md_base;
	struct strace_info* curr=head_ls->next;
	int fd = head_ls->strace_fd;
	if (ctx == NULL){
		return -EINVAL;
	}
	if (head_ls->is_traced == 0){
		return 0;    
	}

	int mode = head_ls->tracing_mode;

	if(mode == FILTERED_TRACING){
		while(curr!= NULL){
			if(curr->syscall_num == syscall_num){
				break;
			}
			curr=curr->next;
		}
		if(curr == NULL) return 0; 
	}

	u64 arr_write[]={syscall_num,param1,param2,param3,param4};
	trace_buffer_write_2(ctx->files[fd],(char*)arr_write,8*(get_no_arguments(syscall_num)+1));
	return 0;
}

int sys_strace(struct exec_context *current, int syscall_num, int action)
{

	if(current == NULL){
		return -EINVAL;
	}

	struct  strace_info* new_node=(struct strace_info*)os_alloc(sizeof(struct strace_info));
	new_node -> syscall_num = syscall_num;
	new_node -> next = NULL;

	if(current->st_md_base == NULL){
		struct strace_head* head_list = (struct strace_head*)os_alloc(sizeof(struct strace_head));
		current->st_md_base = head_list;
		head_list -> is_traced = 0;
		if(action == ADD_STRACE){
			head_list->count=1;
			head_list->next=new_node;
			head_list->last=new_node;
		}
		else{
			return -EINVAL;
		}
		return 0;
	}
	
	// Now we come to the case that the head_list is not NULL 
	struct strace_head* head_ls=current->st_md_base;
	struct strace_info* curr=current->st_md_base->next;
	struct strace_info* prev=NULL;


	if(action == REMOVE_STRACE){
		int found = 0;
		while(curr!= NULL && found==0){
			if(curr->syscall_num == syscall_num){
				found=1;
				head_ls->count--;
				if(prev==NULL){
					head_ls->next = curr->next; // take care of NULL cases 
				}
				else{
					prev->next = curr->next;
				}
				if(curr->next == NULL){
					head_ls->last=prev;
				}
				os_free(curr,sizeof(struct strace_info));
			}
			else{
				prev=curr;
				curr=curr->next;
			}
		}
		if(found == 0) return -EINVAL;
	}
	if(action == ADD_STRACE){
		if(head_ls->count == MAX_STRACE){
			return -EINVAL;
		}
		while(curr!= NULL){
			if(curr->syscall_num == syscall_num){
				return -EINVAL;
			}
			prev=curr;
			curr=curr->next;
		}
		head_ls->count++;
		curr=head_ls->next;
		new_node->next=curr;
		head_ls->next=new_node;

	}

	return 0;
}

int sys_read_strace(struct file *filep, char *buff, u64 count)
{
	if (filep == NULL){
		return -EINVAL;
	}

	struct trace_buffer_info *curr_trace_buffer = filep->trace_buffer;
	if(curr_trace_buffer == NULL) return -EINVAL;
	if(curr_trace_buffer->empty) return 0;
	
	int new_count=0;
	int ro=curr_trace_buffer->read_offset;
	int wo=curr_trace_buffer->write_offset;

	for(int i=count;i>0;i--){
		u64 syscall_num=*(u64*)(curr_trace_buffer->buffer+ro);
		new_count += (get_no_arguments(syscall_num)+1)*8;
		//This can be done directly because in piazza assumption is given that there is enough space to be read and write
		ro =(ro+(get_no_arguments(syscall_num)+1)*8)%4096;
	} 
	return trace_buffer_read_2(filep,buff,new_count);
}

int sys_start_strace(struct exec_context *current, int fd, int tracing_mode)
{
	if(current== NULL || fd<0){
		return -EINVAL;
	}
	if(current->st_md_base != NULL){
		struct strace_head* head_list= current->st_md_base;
		head_list -> tracing_mode = tracing_mode;
		head_list -> strace_fd = fd;
		head_list -> is_traced = 1;
		return 0;
	}
	struct strace_head* head_list = (struct strace_head*)os_alloc(sizeof(struct strace_head));
	current->st_md_base = head_list;
	head_list -> tracing_mode = tracing_mode;
	head_list -> strace_fd = fd;
	head_list -> count = 0;
	head_list -> is_traced = 1;
	head_list -> next = NULL;
	head_list -> last = NULL;
	return 0;
}

int sys_end_strace(struct exec_context *current)
{
	if(current == NULL){
		return -EINVAL;
	}
	struct strace_head* head_list = current->st_md_base;
	struct strace_info* curr = head_list->next;
	struct strace_info* nex;
	while(curr!=NULL){
		nex=curr->next;
		os_free(curr,sizeof(struct strace_info));
		curr=nex;
	}
	os_free(head_list,sizeof(struct strace_head));
	current->st_md_base->next=NULL;
	current->st_md_base->last=NULL;
	return 0;
}

///////////////////////////////////////////////////////////////////////////
//// 		Start of ftrace functionality 		      	      /////
///////////////////////////////////////////////////////////////////////////

long do_ftrace(struct exec_context *ctx, unsigned long faddr, long action, long nargs, int fd_trace_buffer)
{
	if(action == ADD_FTRACE){
		if(ctx->ft_md_base->count >= FTRACE_MAX) return -EINVAL;
		struct ftrace_info* curr = ctx->ft_md_base->next;
		while(curr){
			if(curr->faddr==faddr) return -EINVAL;
			curr = curr->next;
		}

		struct ftrace_info* new_node = os_alloc(sizeof(struct ftrace_info));
		ctx->ft_md_base->count++;
		new_node->fd = fd_trace_buffer;
		new_node->next = NULL;
		new_node->num_args = nargs;
		new_node->faddr=faddr;

		struct ftrace_head* head=ctx->ft_md_base;
		
		if(head->last == NULL){
			head->next=new_node;
			head->last=new_node;
		}
		else{
			head->last->next = new_node;
			head->last = new_node;
		}
		
	}
	else if(action == REMOVE_FTRACE){
		struct ftrace_head* head = ctx->ft_md_base;
		if(head->next==NULL && head->last == NULL){
			return -EINVAL;
		}
		struct ftrace_info* node = head->next;
		struct ftrace_info* prev = NULL;
		int flag=0;
		while(node){
			if(node->faddr == faddr){
				if(prev!=NULL) prev->next = node->next;
				else head->next =node->next;
				os_free(node,sizeof(struct strace_info));
				flag=1;
				break;
			}
			node = node->next;
		}
		if(flag == 0) return -EINVAL;
	}
	else if(action == ENABLE_FTRACE){
		struct ftrace_info* curr = ctx->ft_md_base->next;
		int flag=0;
		while(curr){
			if(curr->faddr==faddr){
				u8 * faddres = (u8*)curr->faddr;
				if(*faddres == INV_OPCODE) return 0;
				for(int i=0;i<4;i++){
					curr->code_backup[i] = *(u8*)(faddres+i);
					*(u8*)(faddres+i) = INV_OPCODE;
				}
				flag=1;
				break;
			}
			curr = curr->next;
		}
		if(flag==0){
			return -EINVAL;
		}
	}
	else if(action == DISABLE_FTRACE){
		struct ftrace_info* curr = ctx->ft_md_base->next;
		int flag=0;
		while(curr){
			if(curr->faddr==faddr){
				u8* fad = (u8*)curr->faddr;
				for(int i=0;i<4;i++){
					*(u8*)(fad+i) = curr->code_backup[i];
				}
				flag=1;
				break;
			}
			curr = curr->next;
		}
		if(flag==0){
			return -EINVAL;
		}
	}
	else if(action == ENABLE_BACKTRACE){
		struct ftrace_info* curr = ctx->ft_md_base->next;
		int flag=0;
		while(curr){
			if(curr->faddr==faddr){
				char * faddres = (char*)curr->faddr;
				curr->capture_backtrace=1;
				flag=1;
				for(int i=0;i<4;i++){
					curr->code_backup[i] = *(u8*)(faddres+i);
					*(u8*)(faddres+i) = INV_OPCODE;
				}
				break;
			}
			curr = curr->next;
		}
		if(flag==0){
			return -EINVAL;
		}
	}
	else if(action == DISABLE_BACKTRACE){
		struct ftrace_info* curr = ctx->ft_md_base->next;
		int flag=0;
		while(curr){
			if(curr->faddr==faddr){
				char * faddres = (char*)curr->faddr;
				curr->capture_backtrace=0;
				flag=1;
				for(int i=0;i<4;i++){
					*(u8*)(faddres+i) = curr->code_backup[i];
				}
				break;
			}
			curr = curr->next;
		}
		if(flag==0){
			return -EINVAL;
		}
	}
	else{
		return -EINVAL;
	}
    return 0;
}

//Fault handler
long handle_ftrace_fault(struct user_regs *regs)
{
	struct exec_context* curr = get_current_ctx();
	struct ftrace_info* ti = curr->ft_md_base->next;
	int found=0;
	while(ti){
		if(ti->faddr==regs->entry_rip){
			found=1;
			int k=5;	
			unsigned long faddr=ti->faddr;
			int n=ti->num_args;
			trace_buffer_write_2(curr->files[ti->fd],(char*)(&faddr),8);
			if(n>=1) trace_buffer_write_2(curr->files[ti->fd],(char*)(&regs->rdi),8);
			if(n>=2) trace_buffer_write_2(curr->files[ti->fd],(char*)(&regs->rsi),8);
			if(n>=3) trace_buffer_write_2(curr->files[ti->fd],(char*)(&regs->rdx),8);
			if(n>=4) trace_buffer_write_2(curr->files[ti->fd],(char*)(&regs->rcx),8);
			if(n>=5) trace_buffer_write_2(curr->files[ti->fd],(char*)(&regs->r8),8);
			if(n>=6) trace_buffer_write_2(curr->files[ti->fd],(char*)(&regs->r9),8);

			u64 m= INV_OPCODE;
			trace_buffer_write_2(curr->files[ti->fd],(char*)(&m),8);

			regs->entry_rsp -= 8;
			*((u64*)regs->entry_rsp) = regs->rbp;
			regs->rbp = regs->entry_rsp;
			regs->entry_rip += 4;
			if(ti->capture_backtrace==1){
				trace_buffer_write_2(curr->files[ti->fd],(char*)(&faddr),8);
				u64 ptr = regs->rbp;
				while(*(u64*)(ptr+8) != END_ADDR){
					trace_buffer_write_2(curr->files[ti->fd],(char*)(ptr+8),8);
					ptr = *(u64*)(ptr);
				}
			}
			break;
		}
		ti = ti->next;
	}
	if(found==0){
		return -EINVAL;
	}
    return 0;
}

int sys_read_ftrace(struct file *filep, char *buff, u64 count)
{
	struct exec_context* ctx = get_current_ctx();
	struct strace_head* head_ls=ctx->st_md_base;
	struct trace_buffer_info *curr_trace_buffer = filep->trace_buffer;
	if(curr_trace_buffer == NULL) return -EINVAL;
	int wo=curr_trace_buffer->write_offset;
	int ro=curr_trace_buffer->read_offset;
	
	char *buffer= curr_trace_buffer->buffer;
	int bytes=0;
	char waste[8];

	for(int i=count;i>0;i--){
		while(*(u64*)(buffer+curr_trace_buffer->read_offset)!=INV_OPCODE && curr_trace_buffer->empty==0){
			int x=trace_buffer_read_2(filep,buff+bytes,8);
			if(x<0) return -EINVAL;
			bytes+=x;
		}
		if(curr_trace_buffer->empty==0){
			int y=trace_buffer_read_2(filep,waste,8);
			if(y<0) return -EINVAL;
		}
	}
	return bytes;
}



