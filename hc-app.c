#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>
#include <time.h>
#include <sys/time.h> 
#include <stdint.h>

static int fd = 0;
static  int mod_delay = 9000; /*模块时延*/

enum mod_status{
	TARGET_NONE = 0,
	TARGET_EXIST,
	MACH_SUSPEND,
};

/*获取时间差*/
struct timespec get_time_offset(struct timespec prev,struct timespec cur)
{
	
	long int sec;
	long int nsec;
	struct timespec ret;
	sec  = cur.tv_sec - prev.tv_sec;
	nsec = cur.tv_nsec - prev.tv_nsec;
	
	if(sec > 0 && nsec < 0){
		ret.tv_sec = sec - 1;
		ret.tv_nsec = 1000000000 + nsec;
	} else {
		ret.tv_sec = sec;
		ret.tv_nsec = nsec;
	}
	return ret;
}

//判断时间差是否大于 timeout
//发现漏洞 t 不能存储 1000000*5000 
//换成 uint64_t ?? 
//直接 time.tv_sec*1000000000 数会变大
int get_timeout(struct timespec time,uint64_t timeout)
{
	uint64_t t = 1000000000*(uint64_t)time.tv_sec + time.tv_nsec;
	timeout = timeout*1000000;
	if(t > timeout) 	
		return 1;
	return 0;
}


int main(int argc,char *argv[])
{
	struct input_event vEvent;
	int ret = 0;
	int status = TARGET_NONE;
	int val = 0, per_val = 0;
	struct timespec fall_t, cur_t, off_t;
	
	fd = open("/dev/input/event3",O_RDONLY | O_NDELAY);
	if(fd < 0) {
		printf("open file is fail\n");
		return fd;
	}
	
	while(1) {
		if( read(fd, &vEvent, sizeof(vEvent)) > 0)
		{
			if(vEvent.code == 0x4)
			val = vEvent.value;  //状态变化
		}
		
		if(per_val != val) {
			if(per_val==0 && val ==1) { 	//上升
				if(status == TARGET_NONE) 
					printf("Someone is approaching.\n");
				status = TARGET_EXIST;
			}
			else {   		//下降
				if(status == TARGET_NONE) 
					printf("this is warning \n");
				if(status == TARGET_EXIST)
					clock_gettime(CLOCK_REALTIME, &fall_t);	//记下下降时间		
			//	printf(" fall_t.tv_sec =%u \n",fall_t.tv_sec);
			//	printf(" fall_t.tv_nsec =%u \n",fall_t.tv_nsec);
			}
			per_val = val;
		}
		
		if(status == TARGET_EXIST && val == 0) {
			clock_gettime(CLOCK_REALTIME,&cur_t);
			off_t = get_time_offset(fall_t,cur_t);	
			if(get_timeout(off_t,mod_delay)) {
			//	printf(" off_t.tv_sec =%u \n",off_t.tv_sec);
			//	printf(" off_t.tv_nsec =%u \n",off_t.tv_nsec);
				printf("The man has left.\n");
				status = TARGET_NONE;
			}
		}
			
	}
	
	
	close(fd);		
}

