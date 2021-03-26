#include "D_Main.h"
#include "D_I2C.h"
#include "D_SIO.h"
#include "D_EIO.h"
#define BYTE	unsigned char
#define UINT	unsigned int
#define ULNG	unsigned long

//センサーの設定
#if 1
	//Miyuki
	#define LINE_L CN1 //ラインセンサー左
	#define LINE_R CN2 //ラインセンサー右
	#define LINE_B CN8 //ラインセンサー後ろ
	#define LINE_LIMIT 400 //ライン処理実行時の最低値
	#define USS_L 9 //USS左
	#define USS_R 10 //USS右
	#define PIXY_BALL 1 //PIXY ボールのオブジェクトナンバー
	#define PIXY_GOAL_Y 3 //黄色ゴールのオブジェクトナンバー
	#define PIXY_GOAL_B 2 //青色ゴールのオブジェクトナンバー
#else
	//Uemura
	#define LINE_L CN1
	#define LINE_R CN2
	#define LINE_B CN3
	#define LINE_LIMIT 400
	#define USS_L 9
	#define USS_R 10
	#define PIXY_BALL 1
	#define PIXY_GOAL_Y 3
	#define PIXY_GOAL_B 2
#endif

//global変数
int range = 20, touchLine = 0; //方位直さない範囲, ライン処理用
char last; //ゴール判断
float ConI = 0, EleD = 0; //pid用
ULNG timer[10]; //タイマー用


int chkNum(UINT u, UINT o, UINT val) {//最低値 最大値 値
	if (u < val && val < o)
	{
		return 1;
	} else {
		return 0;
	}
}

void motors(int a, int b, int c, int d) {//可変長引数にしたいけど後回し
	int i;
	gPwm[0]=a;gPwm[1]=b;
	gPwm[2]=c;gPwm[3]=d;
	gPwm[4]=0;gPwm[5]=0;
	for (i = 0; i < 4; ++i)gPwm[i] = gPwm[i] < 0 ? gPwm[i]*-1+128 : gPwm[i];
	for (i = 0; i < 4; ++i)gPwm[i] = gPwm[i] == 999 ? 128 : gPwm[i];
	pwm_out();
}

ULNG getTimer(int num) {
	printf("get timer!!:%ld\r\n", get_timer((BYTE)0) - timer[num]);
	return get_timer((BYTE)0) - timer[num];
}

int startTimer(int num) {
	printf("start timer!!\r\n");
	timer[num] = get_timer((BYTE)0);
	return 0;
}

int setupTimer(void) {
	int i;
	printf("setup timer!!\r\n");
	clr_timer((BYTE)0);
	for (i = 0; i < sizeof(timer) / sizeof(ULNG); ++i)timer[i] = get_timer((BYTE)0);
	return 0;
}

UINT getLine(int num) {
	int temp[3] = {LINE_L, LINE_R, LINE_B};
	return gAD[temp[num]];
}


UINT getUss(int num) {
	int n[3] = {0, USS_L, USS_R};
	return get_ping(n[num]);
}

UINT* getUsses(void){
	UINT* uss[3]={0, get_ping(USS_L), get_ping(USS_R)};
	return uss;
}

void kick(void) { //燃えないように3秒以上開ける
	set_Led(1, LED_ON);
	if (getTimer(4) > 3000L) {
		motor(100, 100);
		wait_ms(100);
		motor(-100, -100);
		startTimer(4);
	}
	set_Led(1, LED_OFF);
}


// Pixy
int getPixy(int num, UINT *p) {
	// x, y, w, h, s
	p[0] = get_pixydat_x(num); p[1] = get_pixydat_y(num); p[2] = get_pixydat_w(num); p[3] = get_pixydat_h(num); p[4] = p[2] * p[3];
	if (sizeof(p) / sizeof(UINT)==6 && gV[VAR_G] == num)
	{
		p[5]=1;
	}
	return 0;
}

int chkPixy(UINT* ball) {
	UINT x, y;
	x = ball[0]; y = ball[1];
	if (chkNum(140, 180, x) && chkNum(0, 70, y)) {
		return 1;
	} else if (chkNum(0, 140, x) && chkNum(0, 70, y)) {
		return 2;
	} else if (chkNum(0, 160, x) && chkNum(70, 110, y)) {
		return 4;
	} else if (chkNum(0, 140, x) && chkNum(110, 200, y)) {
		return 6;
	} else if (chkNum(180, 320, x) && chkNum(0, 70, y)) {
		return 3;
	} else if (chkNum(160, 320, x) && chkNum(70, 110, y)) {
		return 5;
	} else if (chkNum(180, 320, x) && chkNum(110, 200, y)) {
		return 7;
	} else if (chkNum(140, 180, x) && chkNum(110, 200, y)) {
		return 8;
	} else {
		return 0;
	}
}


void dir() {
	float Dev1, Dev2, ConP;
	startTimer(0);
	gV[VAR_B] = get_bno(0);
	Dev1 = gV[VAR_B] - gV[VAR_V];
	if (Dev1 > 180) {
		Dev1 = Dev1 - 360;
	} else if (Dev1 < -179) {
		Dev1 = Dev1 + 360;
	}
	gV[VAR_C] = get_bno(0);
	Dev2 = gV[VAR_C] - gV[VAR_V];
	if (Dev2 > 180) {
		Dev2 = Dev2 - 360;
	} else if (Dev2 < -179) {
		Dev2 = Dev2 + 360;
	}
	startTimer(1);
	gV[VAR_T] = getTimer(T1) - getTimer(T2);
	if (gV[VAR_M] > getTimer(T3)) {
		ConI = ConI + Dev1;
	} else {
		ConI = 0;
		startTimer(2);
	}
	ConP = Dev1 * 0.3024;
	ConI = ConI * 0.0060;
	EleD = (Dev2 - Dev1) / gV[VAR_T];
	gV[VAR_O] = ConP + ConI + (EleD * 40.00);
	if (gV[VAR_O] < -30) {
		gV[VAR_O] = -30;
	} else if (gV[VAR_O] > 30) {
		gV[VAR_O] = 30;
	}
	gPwm[0] = gV[VAR_O] < 0 ? gV[VAR_O] * -1 : gV[VAR_O] | 0x80;
	gPwm[1] = gV[VAR_O] < 0 ? (gV[VAR_O] * -1) | 0x80 : gV[VAR_O];
	gPwm[2] = gV[VAR_O] < 0 ? gV[VAR_O] * -1 : gV[VAR_O] | 0x80;
	gPwm[3] = gV[VAR_O] < 0 ? (gV[VAR_O] * -1) | 0x80 : gV[VAR_O];
	pwm_out();
}


int processingLine(int num, int stop, UINT* ball) {
	UINT i, y[6], b[6], l[6], *uss, back[6] = {30, 30, 30, 30, 0, 0};
	getPixy(PIXY_GOAL_Y, y); getPixy(PIXY_GOAL_B, b);
	uss=getUsses();
	range=10;
	if(y[5]<b[5]){
		for(i=0;i<sizeof(b) / sizeof(UINT);i++)l[i]=b[i];
	}else{
		for(i=0;i<sizeof(y) / sizeof(UINT);i++)l[i]=y[i];
	}
	if (stop) {
	//if (stop && getTimer(5) > 1000L) {
		gPwm[0] = gPwm[1] = gPwm[2] = gPwm[3] = 128;
		pwm_out();
		wait_ms(500);
	}
	if(uss[3] > uss[1] && uss[3] > uss[2])
	{
		if (last=='y')
		{
			if (PIXY_GOAL_Y==gV[VAR_G])
			{
				if(uss[1] > uss[2]){
					motors(-20,0,0,-20);
				}else{
					motors(0,-20,-20,0);
				}
			}else{
				if(uss[1] > uss[2]){
					motors(20,0,0,20);
				}else{
					motors(0,20,20,0);
				}
			}
		}else{
			if (PIXY_GOAL_B==gV[VAR_G])
			{
				motors(-20,-20,-20,-20);
			}else{
				motors(20,20,20,20);
			}
		}
	}else if(getUss(1)+getUss(2)<50){
	}else if(uss[1] > uss[2]){
		motors(-20,20,20,-20);
	}else{
		motors(20,-20,-20,20);
	}
}

void processingGoal(int num, UINT* ball, int noback) {
	UINT i, y[6], b[6], goal[5];
	getPixy(PIXY_GOAL_Y, y);
	getPixy(PIXY_GOAL_B, b);
	if(y[5]){
		for(i=0; i<5; i++)goal[i]=y[i];
	}else{
		for(i=0; i<5; i++)goal[i]=b[i];
	}
	last = y[5]>b[5] ? 'y' : 'b';
	range = 20;
	if (num == 1) {
		if (chkNum(155, 160, ball[0]) && chkNum(60, 70, ball[1]) && //?{?[???E’u
				chkNum(goal[0] - (goal[2] / 2), goal[0] + (goal[2] / 2), ball[0])) { //?S?[???E’u
			//if (goal[4] > 450 && chkNum(155, 160, ball[0]) & chkNum(60, 70, ball[1])) {
			printf("kick!!\r\n");
			kick();
		}
		if (goal[4]>300)
		{

			motors(30,30,30,30);
		}else{
			motors(40,40,40,40);
		}
	} else if (num == 2) {
		if (noback) {
			gPwm[0] = gPwm[1] = gPwm[2] = gPwm[3] = 35;
			pwm_out();
		} else {
			gPwm[0] = 35;
			gPwm[1] = 35 | 0x80;
			gPwm[2] = 35 | 0x80;
			gPwm[3] = 35;
			pwm_out();
		}
	} else if (num == 3) {
		if (noback) {
			gPwm[0] = gPwm[1] = gPwm[2] = gPwm[3] = 35;
			pwm_out();
		} else {
			gPwm[0] = 35 | 0x80;
			gPwm[1] = 35;
			gPwm[2] = 35;
			gPwm[3] = 35 | 0x80;
			pwm_out();
		}
	} else if (num == 4 || num == 5 || num == 6 || num == 7) {
		gPwm[0] = 35 | 0x80;
		gPwm[1] = 35 | 0x80;
		gPwm[2] = 35 | 0x80;
		gPwm[3] = 35 | 0x80;
		pwm_out();
	} else if (num == 8) {
		UINT y[5], b[5], l; getPixy(PIXY_GOAL_Y, y); getPixy(PIXY_GOAL_B, b);
		if (y[4] != 0 && b[4] != 0) {
			l = b[4] > y[4] ? b[0] : y[0];
			if (l < 160) {
				gPwm[0] = 40;
				gPwm[1] = 40 | 0x80;
				gPwm[2] = 40 | 0x80;
				gPwm[3] = 40;
				pwm_out();
			} else {
				gPwm[0] = 20 | 0x80;
				gPwm[1] = 20;
				gPwm[2] = 20;
				gPwm[3] = 20 | 0x80;
				pwm_out();
			}
			wait_ms(100);
		}
	} else if (num == 0) {
		gPwm[0] = 0 | 0x80;
		gPwm[1] = 0 | 0x80;
		gPwm[2] = 0 | 0x80;
		gPwm[3] = 0 | 0x80;
		pwm_out();
	} else {
		gPwm[0] = 0 | 0x80;
		gPwm[1] = 0 | 0x80;
		gPwm[2] = 0 | 0x80;
		gPwm[3] = 0 | 0x80;
		pwm_out();
	}
	wait_ms(100);
}



void start(void)
{
	int n1;
	// setup
	UINT y[5], b[5];
	set_Led(3, LED_ON);
	wait_ms(8000);
	gV[VAR_V] = get_bno(0);
	set_Led(3, LED_OFF);
	getPixy(PIXY_GOAL_Y, y); getPixy(PIXY_GOAL_B, b);
	//printf("Y:%4ld, B:%4ld", y[4], b[4]);
	if (y[4] < b[4]) {
		gV[VAR_G] = PIXY_GOAL_Y;
		set_Led(0, LED_ON);
	} else {
		gV[VAR_G] = PIXY_GOAL_B;
		set_Led(1, LED_ON);
	}
	wait_ms(2000);
	for (n1 = 0; n1 < 4; n1++) {
		set_Led(n1, LED_OFF);
	}
}

void user_sub_30(void){ //割り込み
	int n1;
	for (n1 = 0; n1 < 3; n1++) {
		if (getLine(n1) > LINE_LIMIT) {
			touchLine = (n1 + 1) * -1;
			printf("%d\r\n", touchLine);
			break;
		}else{
			touchLine=0;
		}
	}
}

void user_main(void)
{
	UINT ball[5]; int stop = 0;
	setupTimer();
	user_sub_30();
	start();
	kick();
	while (TRUE) {
		if (judge_bno(0, gV[VAR_V], range)) {
			if (touchLine == 0) {
				getPixy(PIXY_BALL, ball);
				processingGoal(chkPixy(ball), ball, 0);
				stop = 0;
			} else {
				if (stop==0)
				{
					stop = 1;
					processingLine(touchLine, 1, ball);
				}else{
					processingLine(touchLine, 0, ball);
				}
			}
		} else {
			dir();
		}
	}
}








