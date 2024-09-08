<h1>Emulation of a Simplified Scheduling Policy</h1>
------------------------------------------------------------
<b>Notes:</b>
- Based on Linux Completely Fair Scheduler  
- Uses Producer/Consumer model using threads
- Threads 1 - 4 are the Consumer CPU threads
- Thread 0 is the Producer which adds to the queues of the cpus
- Ensure that 'process_info.txt' is in the directory with the information in Appendix A
- It might take some time for all the processes to complete execution
------------------------------------------------------------
 
Steps to Emulate Simplifed Scheduling Policy:
------------------------------------------------------------
1. Navigate to the terminal and enter the directory of the PC Model
2. Enter "./script" into the terminal (do not include the quotation marks)
3. The system will open 1 xterm windows
4. Resize the xterm window to use at least the whole width of the screen to make the output messages as clean and organized as possible
5. Enter "./pc_demo" in the  xterm window (do not include the quotation marks)
6. The emulation is completed when 'All done' is printed
------------------------------------------------------------

Appendix A:
------------------------------------------------------------
- This is the information 'process_info.txt' should contain:
	1000,NORMAL,120,6100,-1 
	1001,NORMAL,100,4000,-1 
	1002,FIFO,40,5800,0 
	1003,RR,33,9700,3 
	1004,NORMAL,130,6700,-1 
	1005,NORMAL,115,5500,-1 
	1006,FIFO,70,4000,3 
	1007,FIFO,25,2900,-1 
	1008,RR,35,6800,2 
	1009,FIFO,50,7200,-1 
	1010,RR,48,8000,-1 
	1011,NORMAL,135,5300,-1 
	1012,NORMAL,109,2900,-1 
	1013,NORMAL,107,6800,3 
	1014,RR,22,5500,-1 
	1015,NORMAL,137,5000,-1 
	1016,NORMAL,108,7300,-1 
	1017,NORMAL,100,3200,-1 
	1018,NORMAL,132,7400,-1 
	1019,NORMAL,123,3300,-1
------------------------------------------------------------
