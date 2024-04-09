#include <stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sys/mman.h>
#include <unistd.h>
#include<stdbool.h>

void *head=NULL;


void *memalloc(unsigned long size) 
{
	printf("memalloc() called\n");
	if(size==0) return NULL;
    void *temp;
	void *curr; 

    curr= head; 
    bool flag=false; 
	unsigned long nwsz; 
	
    while(curr!=NULL){

        if(size%8){
            nwsz=size+ 16 - (size%8);
        }
        else{
            nwsz= size+ 8;
        }
		if(nwsz<24) nwsz=24;

        if(*(unsigned long *)curr >= nwsz){ 
            // then we have to allocate here 
            flag=true;

			void *prev= *(void **)(curr+16);
            void *nex= *(void **)(curr+8);

            if(*(unsigned long *)curr - nwsz <24){
                // now I have to remove this memory chunk from the list 

				if(prev == NULL){
					head=nex;
					if(nex) *(void **)(nex+16)=NULL;
				}
				else{
					*(void **)(prev + 8)= nex;
					if(nex) *(void **)(nex + 16)=prev;
				}

            }
            else{ // the extra space is greater than equal to 24 bytes 

                void *newcurr= (curr + nwsz); 
                *(unsigned long *)newcurr=( *(unsigned long *)(curr) -nwsz);

				if(prev == NULL){
					head=nex;
					if(nex) *(void **)(nex+16)=NULL;
				}
				else{
					*(void **)(prev + 8)= nex;
					if(nex) *(void **)(nex + 16)=prev;
				}
				void *t=head;
				head=newcurr;
				*(void **)(head+16)=NULL;
				*(void **)(head+8)=t;
				if(t) *(void **)(t+16)=head;

            }

			// lets print 
			//printf("%p\n",curr+8);
            *(unsigned long *)curr=nwsz;
            return (void *)(curr+8);
            
        }

        curr= *(void **)(curr+8);
        
    }
    
    if(!flag){  
        unsigned long nsz=size +8;
		if(nsz<24) nsz=24;
        // size to be allocated using OS 
        unsigned long alc;

		(nsz%(1024*4096) == 0) ? (alc=nsz) : (alc=1024*4096*(1+(nsz/(1024*4096))));


        temp=mmap(NULL,alc,PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_PRIVATE, 0, 0);
		if(temp== MAP_FAILED) return NULL;
        curr=temp;

		(size%8) ? (nwsz=size+ 16 - (size%8)) : (nwsz= size+ 8);
		if(nwsz<24) nwsz=24;
        
        if(alc-nwsz >=24) { // the extra space is greater than equal to 24 bytes

            void *newcurr= curr+nwsz;
            *(unsigned long *)newcurr=( alc - nwsz ); 

            void *t=head;
            head=newcurr;
			*(void **)(head+16)=NULL;
			*(void **)(head+16)=NULL;
			if(t) *(void **)(t+16)=head;


        }
		// else i have to just return 
		//printf("%p\n",curr+8);
		*(unsigned long *)curr=nwsz;
        return (void *)(curr+8);

    }


	return NULL;
}

int memfree(void *ptr)
{ 

	printf("memfree() called\n");
	if(!ptr) return -1;

	void *l=NULL;
	void *r=NULL;

	void *curr=head;  
	while(curr!=NULL){
		unsigned long csize= *(unsigned long *)curr;
		if(curr + csize == ptr-8){
			l=curr;
		}
		if(ptr-8 + *(unsigned long*)(ptr-8) == curr){
			r=curr;
		}
	
		curr=*(void **)(curr+8);
	}
 
	void *t= head;

	void* newst=ptr-8;
	head=newst;
	void* m1=NULL;
	void* m3=NULL;
	*(void **)(head+16)=NULL; // head ka previous is always null
	unsigned long i1=*(unsigned long *)newst;
	unsigned long i2=0;
	unsigned long i3=0;


	if(l==NULL && r==NULL){
		head=newst;  // new wala head is ptr-8
		*(void **)(head+8)=t; //head ka next is purana head
		*(void **)(head+16)=NULL; //head ka prev is null
		if(t!=NULL) *(void **)(t+16)=head; //purane head ka prev is head
		return 0;
	}

	if(l==NULL && r!=NULL){
		i2=*(unsigned long *)r;
		*(unsigned long *)newst=i1+i2;

		m1=*(void **)(r+16);  //jo right hai uska uska previous wala
		m3=*(void **)(r+8);   // jo right hai uska next wala

	}

	if(l!=NULL && r== NULL){
		i2=*(unsigned long *)l;
		*(unsigned long *)l=i1+i2; 

		newst=l;
		head=newst;
		m1=*(void **)(l+16);  //jo left hai uska uska previous wala
		m3=*(void **)(l+8);   // jo left hai uska next wala


	}

	if(l!=NULL && r!=NULL){
		i2=*(unsigned long *)r;
		i3=*(unsigned long *)l;
		*(unsigned long *)l=i1+i2+i3;

		newst=l;
		head=newst;
		m1=*(void **)(l+16);  //jo left hai uska uska previous wala
		m3=*(void **)(r+8);   // jo left hai uska next wala


	}
	*(void **)(head+16)=NULL;

	if(!m1){
		*(void **)(head+8)=m3;
		if(m3) *(void **)(m3+16)=head;
	}
	else{
		*(void **)(m1+8)=m3;  //m1 ka next is now m3 because m2 is gone
		if(m3) *(void **)(m3+16)=m1;  // m3 ka prev is m1 now as m2 is gone 
		*(void **)(head+8)=t; 
		*(void **)(t+16)=head;
	}

	return 0;
}	




















