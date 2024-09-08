#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
#include <math.h>
#include <pthread.h>
#include <sys/time.h> 



#define NUM_THREADS 5
#define FILE_NAME "process_info.txt"
#define BUFFER_SIZE 256
#define QUEUE_SIZE 5
#define NUM_OF_QUEUES 3
#define MAX_SLEEP_AVG 10
#define MIN_SLEEP_AVG 0


struct process_info_st {
    int pid;
    int sp,
    	dp;
    int remain_time;		// in ms 
    int time_slice,
        accu_time_slice;
    pid_t last_cpu;
    char sched_policy[7];
    struct timeval sleep_start;
    int sleep_avg;
};

struct queue_st {		//5 slots in each queue
    struct process_info_st queue[QUEUE_SIZE];
    int size;
};

struct cpu_queues_st {		//3 ready queues per cpu
    struct queue_st ready_queues[NUM_OF_QUEUES];
    int rr_count,
        fifo_count,
        normal_count,
        process_count;
};

struct cpu_queues_st process_q[4];	//1 process queue for each cpu, each cpu has 3 ready queues with 3 slots each
pthread_mutex_t queue_mutex[4][3];	//mutex for each RQ of each cpu

			      
void *thread_function(void *arg);


void add_process(int cpu_affinity, int rq, int pid, int sp, int exec_time, char sched_policy[]) {
    pthread_mutex_lock(&queue_mutex[cpu_affinity][rq]);
    int size = process_q[cpu_affinity].ready_queues[rq].size;
    process_q[cpu_affinity].ready_queues[rq].queue[size].pid = pid;
    process_q[cpu_affinity].ready_queues[rq].queue[size].sp = sp;
    process_q[cpu_affinity].ready_queues[rq].queue[size].dp = sp;		//dp will start off equal to sp
    process_q[cpu_affinity].ready_queues[rq].queue[size].remain_time = exec_time;
    process_q[cpu_affinity].ready_queues[rq].queue[size].last_cpu = 0;	//producer is cpu 0
    sprintf(process_q[cpu_affinity].ready_queues[rq].queue[size].sched_policy, "%s", sched_policy);
    process_q[cpu_affinity].ready_queues[rq].queue[size].sleep_avg = 0;	
    process_q[cpu_affinity].ready_queues[rq].size += 1;
    process_q[cpu_affinity].process_count += 1;
    printf("Process added to RQ%i of CPU %i:\npid->%d, sp->%d, remain_time->%d ms, sched_policy->%s\n\n", rq, cpu_affinity+1, pid, sp, exec_time, sched_policy);
    
    //start timer for sleep time of process
    gettimeofday(&process_q[cpu_affinity].ready_queues[rq].queue[size].sleep_start, NULL);
    pthread_mutex_unlock(&queue_mutex[cpu_affinity][rq]);
}


void add_to_queue() {
    FILE *fp;
    int file_size,				//holds total bytes of the file	
        bytes_read;
    char buffer[256];
    int i, j, k;
    int pid, sp, exec_time, cpu_affinity;
    char sched_policy[7];
        
    // --------------- File Reading Preparation ---------------
    fp = fopen(FILE_NAME, "r");
    if(fp == NULL) {
      perror("Error opening file");
    }
   
    fseek(fp, 0, SEEK_END);
    file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    // --------------- Reading file and adding to buffer ---------------
    if (file_size) {
        for (i = 0; i < 20; i++) {
            
            if (fgets(buffer, BUFFER_SIZE, fp) != NULL) {
                sscanf(buffer, "%d,%[^,],%d,%d,%d", &pid, sched_policy, &sp, &exec_time, &cpu_affinity);		//read the cpu affinity
                if (strncmp(sched_policy, "NORMAL", 6) == 0) {
                    if ((cpu_affinity >= 0) && (cpu_affinity <= 3)) {		//put process in a specific cpu
                        if (process_q[cpu_affinity].normal_count < 3) {
                            if ((sp >= 100) && (sp <= 129)) {		//in RQ1
                                if (process_q[cpu_affinity].ready_queues[1].size < QUEUE_SIZE) {
                                    add_process(cpu_affinity, 1, pid, sp, exec_time, sched_policy);
                                    process_q[cpu_affinity].normal_count += 1;
                                }
                                else printf("\n(ERROR) RQ1 for CPU %d is full\n", cpu_affinity+1);
                            }
                            else if ((sp >= 130) && (sp <= 139)) {
                                if (process_q[cpu_affinity].ready_queues[2].size < QUEUE_SIZE) {	
                                    add_process(cpu_affinity, 2, pid, sp, exec_time, sched_policy);
                                    process_q[cpu_affinity].normal_count += 1;
                                }
                                else printf("\n(ERROR) RQ2 for CPU %d is full\n", cpu_affinity+1);
                            }
                        }
                        else printf("\n(ERROR) Max capacity of processes with NORMAL sched_policy reached for CPU %i\n", cpu_affinity+1);
                    }
                    else if (cpu_affinity == -1) {	//find a cpu queue to put a process
                        for (j = 0; j < 4; j++) {
                            if (process_q[j].normal_count < 3) {
                                if ((sp >= 100) && (sp <= 129)) {		//in RQ1
	                            if (process_q[j].ready_queues[1].size < QUEUE_SIZE) {
	                                add_process(j, 1, pid, sp, exec_time, sched_policy);
	                                process_q[j].normal_count += 1;
                                        break;
	                            }
	                            else printf("\n(here%iERROR) %d RQ1 for CPU %d is full\n", j, pid, cpu_affinity+1);
                                }
                                else if ((sp >= 130) && (sp <= 139)) {
                                    if (process_q[j].ready_queues[2].size < QUEUE_SIZE) {	
                                        add_process(j, 2, pid, sp, exec_time, sched_policy);
                                        process_q[j].normal_count += 1;
                                        break;
                                    }
                                    else printf("\n(ERROR) RQ2 for CPU %d is full\n", cpu_affinity+1);
                                }
                            }
                        
                        }
                    
                    }
                      
                }				//end of adding NORMAL processes
                else {
                    if ((cpu_affinity >= 0) && (cpu_affinity <= 3)) {		//put process in a specific cpu
                        if ((strncmp(sched_policy, "RR", 2) == 0)) {
                            if (process_q[cpu_affinity].rr_count < 1) {		
                                if (process_q[cpu_affinity].ready_queues[0].size < QUEUE_SIZE) {
                                    add_process(cpu_affinity, 0, pid, sp, exec_time, sched_policy);
                                    process_q[cpu_affinity].rr_count += 1;
                                }
                                else printf("\n1(ERROR) RQ0 for CPU %d is full\n", cpu_affinity+1);
                            }
                            else printf("\n(ERROR) Max capacity of processes with RR sched_policy reached for CPU %i\n", cpu_affinity+1);
                        }
                        else if ((strncmp(sched_policy, "FIFO", 4) == 0)) {
                            if (process_q[cpu_affinity].fifo_count < 1) {		
                                if (process_q[cpu_affinity].ready_queues[0].size < QUEUE_SIZE) {
                                    add_process(cpu_affinity, 0, pid, sp, exec_time, sched_policy);
                                    process_q[cpu_affinity].fifo_count += 1;
                                }
                                else printf("\n2(ERROR) RQ0 for CPU %d is full\n", cpu_affinity+1);
                            }
                            else printf("\n(ERROR) Max capacity of processes with FIFO sched_policy reached for CPU %i\n", cpu_affinity+1);
                        }
                    }
                    else if (cpu_affinity == -1) {	//find a cpu queue to put a process
                        if ((strncmp(sched_policy, "RR", 2) == 0)) {
                            for (j = 0; j < 4; j++) {
                                if (process_q[j].rr_count < 1) {		
                                    if (process_q[j].ready_queues[0].size < QUEUE_SIZE) {
                                        add_process(j, 0, pid, sp, exec_time, sched_policy);
                                        process_q[j].rr_count += 1;
                                        break;
                                    }
                                    else printf("\n3(ERROR) RQ0 for CPU %d is full\n", j+1);
                                } 
                            }
                        }
                        else if ((strncmp(sched_policy, "FIFO", 4) == 0)) {
                            for (j = 0; j < 4; j++) {
                                if (process_q[j].fifo_count < 1) {		
                                    if (process_q[j].ready_queues[0].size < QUEUE_SIZE) {
                                        add_process(j, 0, pid, sp, exec_time, sched_policy);
                                        process_q[j].fifo_count += 1;
                                        break;
                                    }
                                    else printf("\n4(ERROR) RQ0 for CPU %d is full\n", j+1);
                                } 
                            }
                        }
                    
                    }
                }  
            }
            else {
                fprintf(stderr, "file reading failed\n");
            }
        }
    }    
}


void move_process(int cpu, int initial_rq, int initial_pos, int target_rq, int target_pos) {
    //locking mutexes (we don't need to lock the initial rq since it is already locked on function call)
    pthread_mutex_lock(&queue_mutex[cpu][target_rq]);
     
    process_q[cpu].ready_queues[target_rq].queue[target_pos].pid = process_q[cpu].ready_queues[initial_rq].queue[initial_pos].pid;
    process_q[cpu].ready_queues[target_rq].queue[target_pos].sp = process_q[cpu].ready_queues[initial_rq].queue[initial_pos].sp;
    process_q[cpu].ready_queues[target_rq].queue[target_pos].dp = process_q[cpu].ready_queues[initial_rq].queue[initial_pos].dp;
    process_q[cpu].ready_queues[target_rq].queue[target_pos].remain_time = process_q[cpu].ready_queues[initial_rq].queue[initial_pos].remain_time;
    process_q[cpu].ready_queues[target_rq].queue[target_pos].time_slice = process_q[cpu].ready_queues[initial_rq].queue[initial_pos].time_slice;
    process_q[cpu].ready_queues[target_rq].queue[target_pos].accu_time_slice = process_q[cpu].ready_queues[initial_rq].queue[initial_pos].accu_time_slice;
    process_q[cpu].ready_queues[target_rq].queue[target_pos].last_cpu = process_q[cpu].ready_queues[initial_rq].queue[initial_pos].last_cpu;
    sprintf(process_q[cpu].ready_queues[target_rq].queue[target_pos].sched_policy, "%s", process_q[cpu].ready_queues[initial_rq].queue[initial_pos].sched_policy);
    process_q[cpu].ready_queues[target_rq].queue[target_pos].sleep_start = process_q[cpu].ready_queues[initial_rq].queue[initial_pos].sleep_start;
    process_q[cpu].ready_queues[target_rq].queue[target_pos].sleep_avg = process_q[cpu].ready_queues[initial_rq].queue[initial_pos].sleep_avg;
    
    //unlocking mutexes
    pthread_mutex_unlock(&queue_mutex[cpu][target_rq]);
}


void swap_processes(int cpu, int rq, int first_pos, int second_pos) {
    struct process_info_st temp_process_holder;
    
    pthread_mutex_lock(&queue_mutex[cpu][rq]);  //lock current queue
    
    //store 1st process in temp holder
    temp_process_holder.pid = process_q[cpu].ready_queues[rq].queue[first_pos].pid;
    temp_process_holder.sp = process_q[cpu].ready_queues[rq].queue[first_pos].sp;
    temp_process_holder.dp = process_q[cpu].ready_queues[rq].queue[first_pos].dp;
    temp_process_holder.remain_time = process_q[cpu].ready_queues[rq].queue[first_pos].remain_time;
    temp_process_holder.time_slice = process_q[cpu].ready_queues[rq].queue[first_pos].time_slice;
    temp_process_holder.accu_time_slice =process_q[cpu].ready_queues[rq].queue[first_pos].accu_time_slice;
    temp_process_holder.last_cpu = process_q[cpu].ready_queues[rq].queue[first_pos].last_cpu;
    sprintf(temp_process_holder.sched_policy, "%s", process_q[cpu].ready_queues[rq].queue[first_pos].sched_policy);
    temp_process_holder.sleep_start = process_q[cpu].ready_queues[rq].queue[first_pos].sleep_start;
    temp_process_holder.sleep_avg = process_q[cpu].ready_queues[rq].queue[first_pos].sleep_avg;
    
    //put 2nd process in 1st process spot
    process_q[cpu].ready_queues[rq].queue[first_pos].pid = process_q[cpu].ready_queues[rq].queue[second_pos].pid;
    process_q[cpu].ready_queues[rq].queue[first_pos].sp = process_q[cpu].ready_queues[rq].queue[second_pos].sp;
    process_q[cpu].ready_queues[rq].queue[first_pos].dp = process_q[cpu].ready_queues[rq].queue[second_pos].dp;
    process_q[cpu].ready_queues[rq].queue[first_pos].remain_time = process_q[cpu].ready_queues[rq].queue[second_pos].remain_time;
    process_q[cpu].ready_queues[rq].queue[first_pos].time_slice = process_q[cpu].ready_queues[rq].queue[second_pos].time_slice;
    process_q[cpu].ready_queues[rq].queue[first_pos].accu_time_slice = process_q[cpu].ready_queues[rq].queue[second_pos].accu_time_slice;
    process_q[cpu].ready_queues[rq].queue[first_pos].last_cpu = process_q[cpu].ready_queues[rq].queue[second_pos].last_cpu;
    sprintf(process_q[cpu].ready_queues[rq].queue[first_pos].sched_policy, "%s", process_q[cpu].ready_queues[rq].queue[second_pos].sched_policy);
    process_q[cpu].ready_queues[rq].queue[first_pos].sleep_start = process_q[cpu].ready_queues[rq].queue[second_pos].sleep_start;
    process_q[cpu].ready_queues[rq].queue[first_pos].sleep_avg = process_q[cpu].ready_queues[rq].queue[second_pos].sleep_avg;
    
    //put temp in 2nd process
    process_q[cpu].ready_queues[rq].queue[second_pos].pid = temp_process_holder.pid; 
    process_q[cpu].ready_queues[rq].queue[second_pos].sp = temp_process_holder.sp; 
    process_q[cpu].ready_queues[rq].queue[second_pos].dp = temp_process_holder.dp;
    process_q[cpu].ready_queues[rq].queue[second_pos].remain_time = temp_process_holder.remain_time;
    process_q[cpu].ready_queues[rq].queue[second_pos].time_slice = temp_process_holder.time_slice;
    process_q[cpu].ready_queues[rq].queue[second_pos].accu_time_slice = temp_process_holder.accu_time_slice;
    process_q[cpu].ready_queues[rq].queue[second_pos].last_cpu = temp_process_holder.last_cpu;
    sprintf(process_q[cpu].ready_queues[rq].queue[second_pos].sched_policy, "%s", temp_process_holder.sched_policy);
    process_q[cpu].ready_queues[rq].queue[second_pos].sleep_start = temp_process_holder.sleep_start;
    process_q[cpu].ready_queues[rq].queue[second_pos].sleep_avg = temp_process_holder.sleep_avg;
    
    pthread_mutex_unlock(&queue_mutex[cpu][rq]);  //unlock current queue
}


void sort_queue(int cpu, int rq) {
    int i, key_dp, j;
    for (i = 0; i < process_q[cpu].ready_queues[rq].size; i++) {
        key_dp = process_q[cpu].ready_queues[rq].queue[i].dp;
        j = i - 1;
 
        // Move elements of arr[0..i-1],
        // that are greater than key, 
        // to one position ahead of their
        // current position
        while (j >= 0 && process_q[cpu].ready_queues[rq].queue[j].dp > key_dp) {
            swap_processes(cpu, rq, j + 1, j);
            j = j - 1;
        }
    }
}



int main() {
  		
  int res;
  pthread_t a_thread[NUM_THREADS];
  /*
  
  0: producer
  1 - 4: consumer
  
  */ 
  void *thread_result;
  int lots_of_threads;
  
  srand(time(NULL));			//initialize random value

  for(lots_of_threads = 0; lots_of_threads < NUM_THREADS; lots_of_threads++) {
    printf("(ALERT) CPU %i created\n", lots_of_threads);
    res = pthread_create(&(a_thread[lots_of_threads]), NULL, thread_function, (void *)(&lots_of_threads));
    if (res != 0) {
      perror("(ERROR) Thread creation failed");
      exit(EXIT_FAILURE);
    }
    sleep(1); 
  }

  printf("(ALERT) Waiting for threads to finish...\n");
  for(lots_of_threads = NUM_THREADS - 1; lots_of_threads >= 0; lots_of_threads--) {
    res = pthread_join(a_thread[lots_of_threads], &thread_result);
    if (res == 0) {
      printf("(ALERT) Picked up a thread\n");
    } else {
      perror("(ERROR) pthread_join failed");
    }
  }



  printf("All done\n");
  
  exit(EXIT_SUCCESS);
}

void *thread_function(void *arg) {
    int my_number = *((int *) arg);
    int i, j, k;
    int temp_dp;		//temporarily holds dp value
    int size;			//holds size/count of each queue temporarily
    char sched_policy[7];
    int real_exec_time, random_exec_time;
    struct timeval temp_timer;

    
    //--------- PRODUCER ---------
    if (my_number == 0) {
        //clearing count for queues
        for (i = 0; i < 4; i++) {
            process_q[i].rr_count = 0;
            process_q[i].normal_count = 0;
            process_q[i].fifo_count = 0;
            process_q[i].process_count = 0;
            //clear count size of each RQ0 RQ1 RQ2            
            process_q[i].ready_queues[0].size = 0;			
            process_q[i].ready_queues[1].size = 0;
            process_q[i].ready_queues[2].size = 0;
            printf("cpu:%i, rr:%i, normal:%i, fifo:%i, rq0:%i, rq1:%i, rq2:%i\n", i, process_q[i].rr_count, process_q[i].normal_count, process_q[i].fifo_count, process_q[i].ready_queues[0].size, process_q[i].ready_queues[1].size, process_q[i].ready_queues[2].size);
        }
            
        add_to_queue();
        
        //sort each queue for each process after each process is added
        for (i = 0; i < 4; i++) {
            sort_queue(i, 0);
            sort_queue(i, 1);
            sort_queue(i, 2);
        }
        
        printf("(ALERT) printing processes sorted in their cpu ready queues:\n\n");
        for (k = 0; k < 4; k++) {
            printf("CPU: %i:\n\n", k+1);
            for (i = 0; i < 3; i++) {
                printf("RQ%i:\n", i);
                for (j = 0; j < 5; j++) {
                    if (process_q[k].ready_queues[i].queue[j].pid == 0) continue;
                    printf("cpu: %i, pid: %i, dp: %i, remain_time: %i ms, time_slice: %i ms, accu_time_slice: %i ms, last_cpu: %i, sched_policy: %s\n", k, process_q[k].ready_queues[i].queue[j].pid, process_q[k].ready_queues[i].queue[j].dp, process_q[k].ready_queues[i].queue[j].remain_time,process_q[k].ready_queues[i].queue[j].time_slice, process_q[k].ready_queues[i].queue[j].accu_time_slice, process_q[k].ready_queues[i].queue[j].last_cpu, process_q[k].ready_queues[i].queue[j].sched_policy);
                }
                printf("\n");
            }
            printf("\n");
        }
    }
    
    //--------- CONSUMER ---------
    else {
        i = 0;
	while (process_q[my_number - 1].process_count > 0) {			//loop thru each ready queue to take ready processes
            pthread_mutex_lock(&queue_mutex[my_number - 1][i]);		//lock the current queue
            size = process_q[my_number - 1].ready_queues[i].size;
            for (j = 0; j < size; j++) {
            	//take time for when a process is selected
            	gettimeofday(&temp_timer, NULL);
            	
                if (process_q[my_number - 1].ready_queues[i].queue[j].remain_time == 0) continue;	//skip process if it's time is 0
 
                sprintf(sched_policy, "%s", process_q[my_number - 1].ready_queues[i].queue[j].sched_policy);
                temp_dp = process_q[my_number - 1].ready_queues[i].queue[j].dp;
                
                //fifo process executes to completion
                if (strncmp(sched_policy, "FIFO", 4) == 0) process_q[my_number - 1].ready_queues[i].queue[j].time_slice = process_q[my_number - 1].ready_queues[i].queue[j].remain_time;
                else {		//RR and NORMAL execute their calculated time slice
                    if (temp_dp < 120) process_q[my_number -1].ready_queues[i].queue[j].time_slice = (140 - temp_dp) * 20;
                    else process_q[my_number -1].ready_queues[i].queue[j].time_slice = (140 - temp_dp) * 5;
                    
                    process_q[my_number -1].ready_queues[i].queue[j].time_slice = fmin(process_q[my_number -1].ready_queues[i].queue[j].time_slice, process_q[my_number - 1].ready_queues[i].queue[j].remain_time);
                }
                
                //we need a calculated dp for RQ1 and RQ2 and also create the "true exection time" simulating i/o action
                if (i) {
                    process_q[my_number - 1].ready_queues[i].queue[j].dp = fmax(100, fmin(temp_dp - process_q[my_number - 1].ready_queues[i].queue[j].sleep_avg + 5, 139));  
                    
                    //(random number from 1-20) * 10
                    random_exec_time = (rand() % (20 - 1 + 1) + 1) * 10;
                    real_exec_time = fmin(process_q[my_number - 1].ready_queues[i].queue[j].time_slice, random_exec_time);
                    
                    //adjust sleep avg
                    process_q[my_number - 1].ready_queues[i].queue[j].sleep_avg -= real_exec_time / 200;
                    process_q[my_number - 1].ready_queues[i].queue[j].sleep_avg += ((temp_timer.tv_sec * 1000 + temp_timer.tv_usec / 1000) - (process_q[my_number - 1].ready_queues[i].queue[j].sleep_start.tv_sec * 1000 +  process_q[my_number - 1].ready_queues[i].queue[j].sleep_start.tv_usec / 1000)) / 200;
                    
                    //make sure sleep_avg is between 0 - 10 ms (bonus can only be between 0 - 10
                    //below or equal 10 ms
                    process_q[my_number - 1].ready_queues[i].queue[j].sleep_avg = fmin(MAX_SLEEP_AVG, process_q[my_number - 1].ready_queues[i].queue[j].sleep_avg);
                    
                    //above or equal 0 ms
                    process_q[my_number - 1].ready_queues[i].queue[j].sleep_avg = fmax(MIN_SLEEP_AVG, process_q[my_number - 1].ready_queues[i].queue[j].sleep_avg);
                    
                    
                    		
                } 
                else real_exec_time = process_q[my_number - 1].ready_queues[i].queue[j].time_slice;
	                
                //update time information
                process_q[my_number - 1].ready_queues[i].queue[j].accu_time_slice += real_exec_time;
                process_q[my_number - 1].ready_queues[i].queue[j].last_cpu = my_number;
                
                //execute process
		usleep(real_exec_time * 1000);
		
		//for NORMAL processes that have a random exec smaller than calculated time, we sleep for 100 ms to simulate moving to block state
		if (i) {
		    if (random_exec_time == real_exec_time) usleep(100 * 1000);
		}
                
                
                //printing updated process info
                printf("cpu: %i, rq: %i, pid: %i, previous dp: %i, updated dp: %i, remain_time: %i ms, expected time_slice: %i ms, real time_slice: %i ms, accu_time_slice: %i ms, last_cpu: %i, sched_policy: %s\n\n", my_number, i, process_q[my_number - 1].ready_queues[i].queue[j].pid, temp_dp, process_q[my_number - 1].ready_queues[i].queue[j].dp, process_q[my_number - 1].ready_queues[i].queue[j].remain_time,process_q[my_number - 1].ready_queues[i].queue[j].time_slice, real_exec_time, process_q[my_number - 1].ready_queues[i].queue[j].accu_time_slice, process_q[my_number - 1].ready_queues[i].queue[j].last_cpu, sched_policy);
                
                process_q[my_number - 1].ready_queues[i].queue[j].remain_time -= real_exec_time;	//take exec time from remaining time
                if (process_q[my_number - 1].ready_queues[i].queue[j].remain_time == 0) process_q[my_number - 1].process_count -= 1;
                
                //start sleep time timer
                gettimeofday(&process_q[my_number - 1].ready_queues[i].queue[j].sleep_start, NULL);
                
                //--- checking if a process needs to move RQ1 <--> RQ2 ---
                // RQ1 --> RQ2
                if ((i == 1) && (process_q[my_number - 1].ready_queues[i].queue[j].dp >= 130)) {
                    printf("(ALERT) PID %i moved from rq1 to rq2\n\n", process_q[my_number - 1].ready_queues[i].queue[j].pid);
                    move_process(my_number - 1, i, j, 2, process_q[my_number - 1].ready_queues[2].size);
                    process_q[my_number - 1].ready_queues[2].size += 1;
                    process_q[my_number - 1].ready_queues[i].size -= 1;
                }
                else if ((i == 2) && (process_q[my_number - 1].ready_queues[i].queue[j].dp < 130)) {
                    printf("(ALERT) PID %i moved from rq2 to rq1\n\n", process_q[my_number - 1].ready_queues[i].queue[j].pid);
                    move_process(my_number - 1, i, j, 1, process_q[my_number - 1].ready_queues[1].size);
                    process_q[my_number - 1].ready_queues[i].size -= 1;
                    process_q[my_number - 1].ready_queues[1].size += 1;
                }
            }
            pthread_mutex_unlock(&queue_mutex[my_number - 1][i]);  //unlock current queue
            
            //only sort for RQ1 and RQ2 since dp does not change for RQ0 processes
            if (i) sort_queue(my_number - 1, i);   //sort queue after being done with it to make sure the highest priority will run first next iteration
            
            i = (i + 1) % NUM_OF_QUEUES;			//increment to next queue	      		
        }    
    }
    
    pthread_exit(NULL);
}

