#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <limits.h>

#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)

#define container_of(ptr, type, member) ({                      \
        const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
        (type *)( (char *)__mptr - offsetof(type,member) );})

#define LIST_POISON1  ((void *) 0x00100100)
#define LIST_POISON2  ((void *) 0x00200200)

struct list_head {
	struct list_head *next, *prev;
};

#define LIST_HEAD_INIT(name) { &(name), &(name) }

#define LIST_HEAD(name) \
	struct list_head name = LIST_HEAD_INIT(name)

#define INIT_LIST_HEAD(ptr) do { \
	(ptr)->next = (ptr); (ptr)->prev = (ptr); \
} while (0)

static inline void __list_add(struct list_head *new,
			      struct list_head *prev,
			      struct list_head *next) {
	next->prev = new;
	new->next = next;
	new->prev = prev;
	prev->next = new;
}

static inline void list_add(struct list_head *new, struct list_head *head) {
	__list_add(new, head, head->next);
}

static inline void list_add_tail(struct list_head *new, struct list_head *head) {
	__list_add(new, head->prev, head);
}

static inline void __list_del(struct list_head * prev, struct list_head * next) {
	next->prev = prev;
	prev->next = next;
}

static inline void list_del(struct list_head *entry) {
	__list_del(entry->prev, entry->next);
	entry->next = LIST_POISON1;
	entry->prev = LIST_POISON2;
}

static inline void list_del_init(struct list_head *entry) {
	__list_del(entry->prev, entry->next);
	INIT_LIST_HEAD(entry);
}

static inline void list_move(struct list_head *list, struct list_head *head) {
        __list_del(list->prev, list->next);
        list_add(list, head);
}

static inline void list_move_tail(struct list_head *list,
				  struct list_head *head) {
        __list_del(list->prev, list->next);
        list_add_tail(list, head);
}

static inline int list_empty(const struct list_head *head) {
	return head->next == head;
}

#define list_entry(ptr, type, member) \
	container_of(ptr, type, member)

#define list_for_each(pos, head) \
  for (pos = (head)->next; pos != (head);	\
       pos = pos->next)

#define list_for_each_prev(pos, head) \
	for (pos = (head)->prev; prefetch(pos->prev), pos != (head); \
        	pos = pos->prev)

#define list_for_each_safe(pos, n, head) \
	for (pos = (head)->next, n = pos->next; pos != (head); \
		pos = n, n = pos->next)

#define list_for_each_entry(pos, head, member)				\
	for (pos = list_entry((head)->next, typeof(*pos), member);	\
	     &pos->member != (head);					\
	     pos = list_entry(pos->member.next, typeof(*pos), member))

#define list_for_each_entry_reverse(pos, head, member)			\
	for (pos = list_entry((head)->prev, typeof(*pos), member);	\
	     &pos->member != (head); 	\
	     pos = list_entry(pos->member.prev, typeof(*pos), member))

#define list_for_each_entry_safe(pos, n, head, member)			\
	for (pos = list_entry((head)->next, typeof(*pos), member),	\
		n = list_entry(pos->member.next, typeof(*pos), member);	\
	     &pos->member != (head); 					\
	     pos = n, n = list_entry(n->member.next, typeof(*n), member))

#define list_for_each_entry_safe_reverse(pos, n, head, member)		\
	for (pos = list_entry((head)->prev, typeof(*pos), member),	\
		n = list_entry(pos->member.prev, typeof(*pos), member);	\
	     &pos->member != (head); 					\
	     pos = n, n = list_entry(n->member.prev, typeof(*n), member))

#if 0    //DEBUG
#define debug(fmt, args...) fprintf(stderr, fmt, ##args)
#else
#define debug(fmt, args...)
#endif

#define PAGESIZE (32) // 페이지 크기
#define PAS_FRAMES (256) //fit for unsigned char frame in PTE // pas의 frame 수 
#define PAS_SIZE (PAGESIZE*PAS_FRAMES) //32*256 = 8192 B
#define VAS_PAGES (64) // 페이지 개수
#define VAS_SIZE (PAGESIZE*VAS_PAGES) //32*64 = 2048 B
#define PTE_SIZE (4) //sizeof(pte)
#define PAGETABLE_FRAMES (VAS_PAGES*PTE_SIZE/PAGESIZE) //64*4/32 = 8 consecutive frames // pagetable의 frame 수
#define PAGE_INVALID (0)
#define PAGE_VALID (1)
#define MAX_REFERENCES (256)



typedef struct{
    unsigned char frame; // 할당된 frame
    unsigned char vflag; // vaild-invaild bit
    unsigned char ref; // 참조 횟수
    unsigned char pad; // padding

} pte; // 페이지테이블 엔트리(4B)

typedef struct{
    int pid; // 프로세스 pid 
    int ref_len; // 페이지 개수 
    unsigned char *references; // 페이지
    struct list_head job; // job이라는 리스트의 노드 생성
} process;

typedef struct {
    unsigned char b[PAGESIZE]; // 페이지가 들어가있는 배열
} frame; 

int pVisted_cnt[65]={0, }; // 프로세스별 방문 횟수 저장
int ref_index[11]; // 레퍼런스 주소 저장
int frame_cnt[11]; // 프로세스별 프레임 사용 개수
int pagefault_cnt[11]; // 프로세스별 페이지폴트 개수 
int arr[11][65]; // 각 프로세스별 레퍼런스와 그때의 방문 횟수 저장 arr[0][0]: 0번 pid의 0번째 레퍼런스의 방문 횟수 =0 
int frame_arr[11][65]; // 각 프로세스별 레퍼런스와 그때 할당된 frame 번호 저장
int allref_cnt;
int cnt=0; // 도는 레퍼런스의 개수 누적
int oom=1; // out of memory의 경우 
int all_pf=0; // 전체 페이지 폴트
int all_ref=0; // 전체 레퍼런스
process p; // process 구조체 p
LIST_HEAD(job_q); // 리스트 헤드 선언
int frame_index=0; // 프레임 index
pte *PTE;

void load(frame* PAS);//프로세스의 정보를 읽어오는 함수
void operation(frame* PAS);//실제 demand Paging을 수행하는 함수
void print_info(frame* PAS);// 동작 후 정보 출력 함수
void clear_process(frame* PAS);// 동적할당 해제 함수
void demand_paging(frame* PAS);// 동작함수들을 묶어 실행하는 함수


int main(int argc,char* argv[]){
    frame* PAS= (frame*)malloc(PAS_SIZE); // PAS 선언   
    demand_paging(PAS);
}
void load(frame* PAS){
    process *cur_pro;
     while(1) // bin에서 읽어옴
    {
        cur_pro = malloc(sizeof(process));
        if(fread(&p, sizeof(int)*2, 1, stdin) == 1)// pid, ref_len 를 읽어옴
        {
            allref_cnt += p.ref_len; // 모든 레퍼런스의 개수를 저장 
            //p의 정보를 cur로 옮겨줌(cur정보 초기화)
            cur_pro-> pid = p.pid;
            cur_pro-> ref_len = p.ref_len;
            cur_pro->references = malloc(sizeof(unsigned char) *cur_pro->ref_len); // cur의 레퍼런스 선언
            INIT_LIST_HEAD(&cur_pro->job); // cur의 job리스트 초기화
            list_add_tail(&cur_pro->job, &job_q); // job_q 리스트와 cur의 job 연결
            for(int i=0; i<cur_pro->ref_len; i++) 
            {
                fread(&cur_pro->references[i], sizeof(char), 1, stdin); // cur의 references 읽어옴

            }
            // 페이지 테이블 만들기
            for(int i=0; i<8; i++) // 페이지테이블은 8칸으로 나누어짐
            {
                pte *PTE = (pte *)PAS[frame_index].b; // PTE생성, PAS의 frame에 넣음

                for(int j=0; j<8; j++) // 1칸에는 8개의 page가 저장
                { 
                    // 각 page의 pte를 초기화
                    PTE[j].frame =0; // j번 페이지의 frame 정보
                    PTE[j].vflag =PAGE_INVALID;
                    PTE[j].ref=0;
                }
                frame_index++;
            }
        } 
        else{ // bin 전부 돌면 나감
            break;
        } 
    }
}
void operation(frame* PAS){
    process *cur_pro;
     while(oom)
    {
        if(cnt == allref_cnt){
            break;
        }

        // PAS 채우기
        list_for_each_entry(cur_pro, &job_q, job) // job_q리스트 순회
        {
            // 레퍼런스 다 돌면 순회 종료
            if(ref_index[cur_pro->pid] == cur_pro->ref_len) // 몇번째 레퍼런스인지(1,2,3...) = 길이 -> 종료
                continue;
            cnt++;

            // 현재 레퍼런스 페이지테이블에 할당해줌
                int cur_ref = cur_pro->references[ref_index[cur_pro->pid]]; // 현재 탐색 중인 레퍼런스 정보(52, 7...)
                int cur_frame = (cur_pro->pid)*8+cur_ref/8; // 레퍼런스가 들어있는 pagetable의 frame 번호
                int cur_pte = cur_ref%8; // 현재 pte값
                PTE = (pte *)PAS[cur_frame].b; // 현재 frame에 PTE 넣음
                if(PTE[cur_pte].vflag == PAGE_INVALID){// 할당되지 않은 페이지일 때(page fault 발생)
                    if(frame_index == MAX_REFERENCES) // 더이상 할당해 줄 프레임이 없을 때 
                    {
                        printf("Out of memory!!\n"); 
                        oom = 0;
                        break;
                    } 
                    else{
                        all_pf++;
                        pagefault_cnt[cur_pro->pid]++; // 프로세스 별 페이지 폴트 개수
                        PTE[cur_pte].frame = frame_index; // 남은 frame을 할당
                        frame_arr[cur_pro->pid][cur_ref] = frame_index; // 프로세스의 레퍼런스에 할당된 frame 정보 저장
                        PTE[cur_pte].vflag = PAGE_VALID; // 페이지 할당됐다고 표시
                        PTE[cur_pte].ref++; // 방문+1 
                        frame_index++; // 프레임 하나 사용
                        frame_cnt[cur_pro->pid]++; // 프로세스 별 할당한 프레임 개수
                        arr[cur_pro->pid][cur_ref]++; // 프로세스의 레퍼런스 방문횟수 +1 
                    }
                }
                else{ // 할당된 페이지
                    PTE[cur_pte].ref++; // 방문+1
                    //printf("Frame %03d\n", PTE[cur_pte].frame);
                    arr[cur_pro->pid][cur_ref]++; // 프로세스의 레퍼런스 방문횟수 +1 
                }
                pVisted_cnt[cur_pro->pid]++; // 프로세스 방문 횟수 +1
                ref_index[cur_pro->pid]++; // 현재 레퍼런스의 작업 위치
                all_ref++; // 모든 프로세스 방문횟수 +1
        }
    }
}
void print_info(frame* PAS){
    process *cur_pro;

    list_for_each_entry(cur_pro, &job_q, job)
    {

        int cur_ref = cur_pro->references[ref_index[cur_pro->pid]]; // 현재 탐색 중인 레퍼런스
        int cur_frame = cur_ref/8; // 레퍼런스가 들어있는 pagetable의 frame 번호
        int cur_pte = cur_ref%8; // 현재 pte값
        PTE = (pte *)PAS[cur_frame].b; // 현재 frame에 PTE 넣음

        printf("** Process %03d: Allocated Frames=%03d PageFaults/References=%03d/%03d\n", cur_pro->pid, frame_cnt[cur_pro->pid]+8, pagefault_cnt[cur_pro->pid], pVisted_cnt[cur_pro->pid]);

        
        for(int j=0; j<64; j++)
        {
            if(arr[cur_pro->pid][j] != 0)
            {
                printf("%03d -> %03d REF=%03d\n", j, frame_arr[cur_pro->pid][j], arr[cur_pro->pid][j]);
            }
        }
    }
    printf("Total: Allocated Frames=%03d Page Faults/References=%03d/%03d\n", frame_index, all_pf, all_ref);
}
void clear_process(frame* PAS){
    process *ptr1,*ptr2;
    
    list_for_each_entry_safe(ptr1, ptr2, &job_q, job){
        list_del(&ptr1->job); // 노드 동적할당 해제
        free(ptr1->references);
        free(ptr1);
    }
    free(PAS);
}
void demand_paging(frame* PAS){
    load(PAS);
    operation(PAS);
    print_info(PAS);
    clear_process(PAS);
}