/*
C 函数声明和JAVA函数声明混合的问题
方便以后上位机需要某个函数功能函数时
直接把此文件中的C函数声明改成JAVA函数就可以了
当然原函数名称也要修改
*/
#ifndef  __FIGNER_H__
#define  __FIGNER_H__

#include <jni.h>

#define    SIZE_H         50
#define    SIZE_L		  17
#define    IC_ADDR        0xffffffff
#define    U_HEAD_H       0X50
#define    U_HEAD_L       0X51
#define    RECV_ADDR      0X52
#define    PACK_SIGN      0X53
#define    PACK_LEN       0X54
#define    CONFIRM_ACK    0X55
#define    ARGUMENT_RET   0X56
#define    CHECK_SUM      0X57
#define    RECV_ERR       0X58
#define    RECV_FINISH    0X59

#define    NO_HEAD        0X0

#define    SUCCESS        0X75
#define    ERR            (-1)
#define    RECORD_ERR     (-2)
#define    READ_ERR       (-3)
#define    DELAYTIME      500000
#define    OVERTIME       10
#define    WAITTIME       5000000

#define    SERSTOP        0x56
#define    SERPAUSE       0X57
#define    VERIFY         0X58
#define    REGISTER       0X59
#define    OPENED         0X60
#define    CLOSED         0x61

//#define    DEBUG    printf("%s  %d\n", __func__, __LINE__)


/*********应答确认码定义**************************/
#define    ACK_OK                0X0
#define    ACK_WRONGPAC          0X01
#define    ACK_NOFINGER          0X02
#define    ACK_RECFAILD          0X03

/*********应答确认码定义**************************/

/*********缓冲区定义**************************/
#define   charbuffer1            0x01
#define   charbuffer2            0x02
/*********缓冲区定义**************************/

/*********包头定义**************************/
#define    PACKET_HEAD           0X01EF
/*********包头定义**************************/

/*********包标志定义**************************/
#define    SIGN_CMD              0X01
#define    SIGN_DATA             0X02
#define    SIGN_END              0X08
/*********包标志定义**************************/

/*********指令集定义**************************/
#define    GETIMAGE      		 0X01
#define    GENCHAR       		 0X02
#define    MATCH         		 0X03
#define    SEARCH        		 0X04
#define    REGMODEL      		 0X05
#define    STORECHAR     		 0X06
#define    LOADCHAR      		 0X07
#define    UPCHAR        		 0X08
#define    DOWNCHAR      		 0X09
#define    UPIMAGE       		 0X0A
#define    DOWNIMAGE     		 0X0B
#define    DELETECHAR     		 0X0C
#define    EMPTY         		 0X0D
#define    WRITEREG      		 0X0E
#define    READSYSPARA   		 0X0F
#define    ENROLL        		 0X10
#define    IDENTIFY      		 0X11 // 验证指纹
#define    SETPWD        		 0X12
#define    VFYPWD        		 0X13
#define    GETRANDOMCODE 		 0X14
#define    SETCHIPADDR   		 0X15
#define    READINFPAGE   		 0X16
#define    PORTCONTROL   		 0X17
#define    WRITENOTEPAD  		 0X18
#define    READNOTEPAD   		 0X19
#define    BURNCODE       	     0X1A
#define    HIGHSPEEDSEARCH  	 0X1B
#define    GENBINIMAGE           0X1C
#define    VALIDTEMPLETENUM      0X1D
/*********指令集定义**************************/

#pragma  pack(1)
typedef struct pack_finger{
    unsigned short  pack_head; // 0xef01
    unsigned int    ic_addr;   // 0xffffffff
    unsigned char   pack_sign; // 1
    unsigned short  pack_len;  // 0x0003
	unsigned char   pack_cmd;  // 0x01
}FINGER_PACK;

#pragma  pack()

extern unsigned long  chip_address;
extern unsigned char  buf_data[SIZE_H];

int  UartInit(void);

int GetImage(void);//1
int GenChar(unsigned char bufid);//2
int Match(void);//3
int Search(char bufid, unsigned short startpage, unsigned short pagenum);//4
int RegModel(void);//5
int StoreChar(char bufid, unsigned short pageid);//6
int LoadChar(char bufid, unsigned short pageid);//7
int UpChar(char bufid);//8
int DownChar(char bufid);//9
int UpImage(void);//10
int DownImage(void);//11
int DeleteChar(unsigned short pageid, unsigned short n);//12
int Empty(void);//13
int WriteReg(char reg, char content);//14
int ReadSysPara(void);//15
int Enroll(void);//16
int Identify(void);//17
int SetPwd(char *password);//18
int VfyPwd(char *password);//19
int GetRandomCode(void);//20
int SetChipAddr(unsigned int addr);//21
int ReadINFpage(void);//22
int PortControl(char con);//23
int WriteNotepad(char pagenum, char *content);//24
int ReadNotepad(char pagenum);//25
int BurnCode(char on);//26
int HighSpeedSearch(char bufid, unsigned short startpage, unsigned short pagenum);//27
int GenBinImage(char type);//28
int ValidTempleteNum(void);//29
void UartClose(void);


int  Java_xwp_jr_Fingerprint_InitFinger(JNIEnv *env, jclass thiz);
int  Java_xwp_jr_Fingerprint_FingerReg(JNIEnv *env, jclass thiz);
int  Java_xwp_jr_Fingerprint_FirstReg(JNIEnv *env, jclass thiz);
int  Java_xwp_jr_Fingerprint_SecondReg(JNIEnv *env, jclass thiz);
int  Java_xwp_jr_Fingerprint_FingerId(JNIEnv *env, jclass thiz);
void Java_xwp_jr_Fingerprint_UartExit(JNIEnv *env, jclass thiz);
// 线程相关
void Java_xwp_jr_Fingerprint_setJNIEnv(JNIEnv *env, jclass thiz);
void Java_xwp_jr_Fingerprint_StartDetectService(JNIEnv *env, jclass thiz);
void Java_xwp_jr_Fingerprint_StopDetectService(JNIEnv *env, jclass thiz);


#endif // __FINGER_H__	

