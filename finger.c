
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <arpa/inet.h>
#include "finger.h"
#include <jni.h>

static int fd = 0;
unsigned long chip_address = 0xffffffff;  // 可改变
unsigned char buf_data[SIZE_H];

static unsigned short big_endian_s(unsigned short data)
{
	return ((0xff00 & data) >> 8) | ((0xff & data) << 8);
}

/* 
打包函数
参数定义：
	buf:包地址   sign:包标志   len: 包长度   cmd :指令
*/
static void mkpacket(void *buf, char sign, short len, char cmd)
{
	FINGER_PACK  *pack = (FINGER_PACK *)buf;

	pack->pack_head = PACKET_HEAD;           // 包头
	pack->ic_addr   = chip_address;          // 芯片地址 
	pack->pack_sign = sign;                  // 包标志
	pack->pack_len  = big_endian_s(len);    // 包长度
	pack->pack_cmd  = cmd;                   // 指令
}

// 此函数要加一个alam信号做定时 防止死循环  还未加
static int ReadCode(void)
{
	unsigned char addr_room[4],recv_space[2],recv_len[2];
	unsigned char tmp = 0, data_num = 0;
	unsigned char recv_ack = 0;
	unsigned char recv_status = 0;
	unsigned char recv_num = 0;
	unsigned int  check_sum = 0;
	int  ret = 0;

	while(1)
	{
		ret = read(fd, &tmp, 1);
		if(ret < 1)
			break;

		if(tmp == 0xef)
		{
			recv_status = U_HEAD_H;
		}

		switch(recv_status){
			case U_HEAD_H:                              // 读取包头高8字节
				recv_status = U_HEAD_L;
				break;

			case U_HEAD_L:                              // 读取包头低8字节
				if(tmp == 0x1)
					recv_status = RECV_ADDR;
				else
					recv_status = RECV_ERR;
				break;

			case RECV_ADDR:                             // 接收ic地址
				addr_room[recv_num++] = tmp;
				if(recv_num >= 4)
				{
					recv_num = 0;
					recv_status = PACK_SIGN;
					
					if(*(unsigned int *)addr_room == IC_ADDR)
						recv_status = PACK_SIGN;
					else
						recv_status = RECV_ERR;
					
				}
				break;

			case PACK_SIGN:                             // 接收包标志
				if(tmp == 0x07)
					recv_status = PACK_LEN;
				else
					recv_status = RECV_ERR;
				break;

			case PACK_LEN:                               // 接收包长度  确认码＋参数 ＋ 2（校验和）
				recv_len[recv_num++] = tmp;
				if(recv_num >= 2)
				{
					recv_num = 0;
					recv_status = CONFIRM_ACK;
				}
				break;

			case CONFIRM_ACK:                            // 确认码
				recv_ack = tmp;
				if(recv_len[1] < 4)  
					recv_status = CHECK_SUM;
				else                                     //判断包长度，大于等于4，说明有参数返回
				{
					data_num = recv_len[1] -3;
					recv_status = ARGUMENT_RET;
				}
				break;

			case ARGUMENT_RET:                           // 接收返回值
				buf_data[--data_num] = tmp;
				if(data_num <= 0)
					recv_status = CHECK_SUM;
				break;

			case CHECK_SUM:
				recv_space[recv_num++] = tmp;
				if(recv_num >= 2)
				{
					recv_num = 0;
					check_sum = *(unsigned short *)recv_space;
					if(check_sum)
						recv_status = RECV_FINISH;
				}					
				break;
			case RECV_ERR:
			case NO_HEAD:
			default:
				break;
		}// case(tmp)

		if(recv_status == RECV_FINISH)
		{
			if(recv_ack == 0)
				ret = SUCCESS;
			else
				ret = recv_ack;  // 把错误码返回

			break;
		}
	}// while(1)

	return ret;
}

/*
功能：关闭串口
*/
void UartClose(void)
{
	close(fd);
}

/*
供JAVA调用函数
功能：初始化串口
*/
int  UartInit(void)
{
	struct termios options;
	int i;

	fd = open("/dev/ttyUSB0", O_RDWR | O_NOCTTY | O_NONBLOCK);
	if(fd < 0)
	{
	    perror("open");
	    return -1;
	}
	
	fcntl(fd, F_SETFL, 0); // set block
	/////set serial//////////////////////////////
	
	
	tcgetattr(fd, &options);
	
	cfsetispeed(&options, B57600);
	cfsetospeed(&options, B57600);
	
	options.c_cflag &= ~PARENB;
	options.c_cflag &= ~CSTOPB;
	options.c_cflag &= ~CSIZE;
	options.c_cflag |= CS8;
	
	options.c_cflag &= ~CRTSCTS; //disable hw flow control
	options.c_cflag |= CREAD | CLOCAL;

	// 设置超时时间
	options.c_cc[VMIN] = 0;
	options.c_cc[VTIME] = 50;

	tcsetattr(fd, TCSANOW, &options);
	/////////////////////////////////////


	return fd;
}


// 1.
/*
从传感器上读入图像存于图像缓冲区
*/
int GetImage(void)
{
	char buf[SIZE_L];
	int ret;
	short len = 0x0003;
	
	mkpacket(buf, SIGN_CMD, len, GETIMAGE);
	// ef01 ffffffff 01 0003 01 0005
	buf[10] = 0;
	buf[11] = 0x05;

	write(fd, buf, 12);
	ret = ReadCode();
	
	return ret;
}


// 2.
/*
根据原始图像生成指纹特征存于charbuffer1 or charbuffer2
bufid: 缓冲区 charbuffer1 or charbuffer2
*/
int GenChar(unsigned char bufid)
{
	char buf[SIZE_L];
	int ret, i;
	short len = 0x04;
	unsigned short check_sum = 0;

	mkpacket(buf, SIGN_CMD, len, GENCHAR);
	buf[10] = bufid;
	for(i = 0; i < len + 1; i++)
		check_sum += buf[i+6];
	buf[11] = (char)((check_sum & 0xff00) >> 8); //
	buf[12] = (char)(check_sum & 0xff);

	write(fd, buf, 13);
	ret = ReadCode();

	return ret;
}

// 3.
/*
精确比对charbuffer1 and charbuffer2 中的特征文件
*/
int Match(void)
{
	char buf[SIZE_L];
	int ret;
	short len = 0x03;
	
	mkpacket(buf, SIGN_CMD, len, MATCH);
	buf[10] = 0;
	buf[11] = 0x07;

	write(fd, buf, 12);
	ret = ReadCode();

	return ret;
}

// 4.
/*
以charbuffer1 或 charbuffer2 中的特征文件搜索整个或部分指纹库
bufid    : 缓冲区charbuffer1 or charbuffer2
startpage: 起始页
pagenum  : 要搜索的页数
*/
int Search(char bufid, unsigned short startpage, unsigned short pagenum)
{
	char buf[SIZE_L];
	int ret, i;
	short len = 0x08;
	unsigned short check_sum = 0;

	mkpacket(buf, SIGN_CMD, len, SEARCH);
	buf[10] = bufid;
	buf[11] = (char)((startpage & 0xff00) >> 8);
	buf[12] = (char)(startpage & 0xff);
	buf[13] = (char)((pagenum & 0xff00) >> 8);
	buf[14] = (char)(pagenum & 0xff);
	for(i = 0; i < len + 1; i++)
		check_sum += buf[i+6];
	buf[15] = (char)((check_sum & 0xff00) >> 8);
	buf[16] = (char)(check_sum * 0xff);

	write(fd, buf, 17);
	ret = ReadCode();

	return ret;
}

// 5.
/*
将charbuffer1 和 charbuffer2中的特征文件合并生成模板存于charbuffer2
*/
int RegModel(void)
{
	char buf[SIZE_L];
	int ret;
	short len = 0x03;

	mkpacket(buf, SIGN_CMD, len, REGMODEL);
	buf[10] = 0;
	buf[11] = 0x09;

	write(fd, buf, 12);
	ret = ReadCode();

	return ret;
}

// 6.
/*
将特征缓冲区的文件储存到flash指纹库中
bufid  :  缓冲区号charbuffer1 or charbuffer2
pageid :  要存储的页
*/
int StoreChar(char bufid, unsigned short pageid)
{
	char buf[SIZE_L];
	int ret, i;
	short len = 0x06;
	unsigned short check_sum = 0;

	mkpacket(buf, SIGN_CMD, len, STORECHAR);
	buf[10] = bufid;
	buf[11] = (char)((pageid & 0xff00) >> 8);
	buf[12] = (char)(pageid & 0xff);
	for(i = 0; i < len + 1; i++)
		check_sum += buf[i+6];
	buf[13] = (char)((check_sum & 0xff00) >> 8);
	buf[14] = (char)(check_sum & 0xff);

	write(fd, buf, 15);
	ret = ReadCode();

	return ret;
}

// 7.
/*
从flash指纹库中读取一个模板到特征缓冲区
bufid  : 缓冲区号 charbuffer1 or charbuffer2
pageid : 要读取的页
*/
int LoadChar(char bufid, unsigned short pageid)
{
	char buf[SIZE_L];
	int ret, i;
	short len = 0x06;
	unsigned short check_sum = 0;

	mkpacket(buf, SIGN_CMD, len, LOADCHAR);
	buf[10] = bufid;
	buf[11] = (char)((pageid & 0xff00) >> 8);
	buf[12] = (char)(pageid & 0xff);
	for(i = 0; i < len + 1; i++)
		check_sum += buf[i+6];
	buf[13] = (char)((check_sum & 0xff00) >> 8);
	buf[14] = (char)(check_sum & 0xff);

	write(fd, buf, 15);
	ret = ReadCode();

	return ret;
}

// 8.
/*
将特征缓冲区的文件上传给上位机
bufid : 缓冲区号 charbuffer1 or charbuffer2
*/
int UpChar(char bufid)
{
	char buf[SIZE_L];
	int ret, i;
	short len = 0x04;
	unsigned short check_sum = 0;

	mkpacket(buf, SIGN_CMD, len, UPCHAR);
	buf[10] = bufid;
	for(i = 0; i < len + 1; i++)
		check_sum += buf[i+6];
	buf[13] = (char)((check_sum & 0xff00) >> 8);
	buf[14] = (char)(check_sum & 0xff);

	write(fd, buf, 15);
	ret = ReadCode();

	return ret;
}

// 9.
/*
从上位机下载一个特征文件到缓冲区
bufid : 缓冲区号 charbuffer1 or charbuffer2
*/
int DownChar(char bufid)
{
	char buf[SIZE_L];
	int ret, i;
	short len = 0x04;
	unsigned short check_sum = 0;

	mkpacket(buf, SIGN_CMD, len, DOWNCHAR);
	buf[10] = bufid;
	for(i = 0; i < len + 1; i++)
		check_sum += buf[i+6];
	buf[13] = (char)((check_sum & 0xff00) >> 8);
	buf[14] = (char)(check_sum & 0xff);

	write(fd, buf, 15);
	ret = ReadCode();
	
	return ret;
}

// 10.
/*
上传原始图像
*/
int UpImage(void)
{
	char buf[SIZE_L];
	int ret;
	short len = 0x03;
	
	mkpacket(buf, SIGN_CMD, len, UPIMAGE);
	buf[10] = 0;
	buf[11] = 0x0e;
	
	write(fd, buf, 12);
	ret = ReadCode();

	return ret;
}

// 11.
/*
上位机下载图像模块给模块
*/
int DownImage(void)
{
	char buf[SIZE_L];
	int ret;
	short len = 0x03;
	
	mkpacket(buf, SIGN_CMD, len, DOWNIMAGE);
	buf[10] = 0;
	buf[11] = 0x0f;
	
	write(fd, buf, 12);
	ret = ReadCode();

	return ret;
}

// 12.
/*
删除flash数据库中指定id号开始的n个指纹模块
pageid : id号
n      : 模块个数
*/
int DeleteChar(unsigned short pageid, unsigned short n)
{
	char buf[SIZE_L];
	int ret, i;
	short len = 0x07;
	unsigned short check_sum = 0;

	mkpacket(buf, SIGN_CMD, len, DELETECHAR);
	buf[10] = (char)((pageid & 0xff00) >> 8);
	buf[11] = (char)(pageid & 0xff);
	buf[12] = (char)((n & 0xff00) >> 8);
	for(i = 0; i < len + 1; i++)
		check_sum += buf[i+6];
	buf[13] = (char)((check_sum & 0xff00) >> 8);
	buf[14] = (char)(check_sum & 0xff);

	write(fd, buf, 15);
	ret = ReadCode();

	return ret;
}

// 13.
/*
清空flash指纹库
*/
int Empty(void)
{
	char buf[SIZE_L];
	int ret, i;
	short len = 0x03;
	
	mkpacket(buf, SIGN_CMD, len, EMPTY);
	buf[10] = 0;
	buf[11] = 0x11;

	write(fd, buf, 12);
	ret = ReadCode();

	return ret;
}

// 14.
/*
写模块寄存器
reg    : 寄存器地址 4/5/6
content: 内容
*/
int WriteReg(char reg, char content)
{
	char buf[SIZE_L];
	int ret, i;
	short len = 0x05;
	unsigned short check_sum = 0;

	mkpacket(buf, SIGN_CMD, len, WRITEREG);
	buf[10] = reg;
	buf[11] = content;
	for(i = 0; i < len + 6; i++)
		check_sum += buf[i+6];
	buf[12] = (char)(check_sum >> 8);
	buf[13] = (char)(check_sum);

	write(fd, buf, 14);
	ret = ReadCode();
	
	return ret;
}

// 15.
/*
读模块寄存器
*/
int ReadSysPara(void)
{
	char buf[SIZE_L];
	int ret;
	short len = 0x03;
	
	mkpacket(buf, SIGN_CMD, len, READSYSPARA);
	buf[10] = 0;
	buf[11] = 0x13;

	write(fd, buf, 12);
	ret = ReadCode();

	return ret;
}

// 16.
/*
注册模板
*/
int Enroll(void)
{
	char buf[SIZE_L];
	int ret;
	short len = 0x03;

	mkpacket(buf, SIGN_CMD, len, ENROLL);
	buf[10] = 0;
	buf[11] = 0x14;

	write(fd, buf, 12);
	ret = ReadCode();

	return ret;
}

// 17.
/*
验证指纹
*/
int Identify(void)
{
	char buf[SIZE_L];
	int ret;
	short len = 0x03;

	mkpacket(buf, SIGN_CMD, len, IDENTIFY);
	buf[10] = 0;
	buf[11] = 0x15;

	write(fd, buf, 12);
	ret = ReadCode();

	return ret;
}

// 18.
/*
设置设备握手口令
password: 口令
*/
int SetPwd(char *password)
{
	char buf[SIZE_L];
	int ret, i;
	short len = 0x07;
	unsigned short check_sum = 0;

	mkpacket(buf, SIGN_CMD, len, SETPWD);
	buf[10] = password[0];
	buf[11] = password[1];
	buf[12] = password[2];
	buf[13] = password[3];
	for(i = 0; i < len + 1; i++)
		check_sum += buf[i+6];
	buf[14] = (char)((check_sum & 0xff00) >> 8);
	buf[15] = (char)(check_sum & 0xff);

	write(fd, buf, 16);
	ret = ReadCode();

	return ret;
}

// 19.
/*
验证设备握手口令
password: 口令
*/
int VfyPwd(char *password)
{
	char buf[SIZE_L];
	int ret, i;
	short len = 0x07;
	unsigned short check_sum = 0;

	mkpacket(buf, SIGN_CMD, len, SETPWD);
	buf[10] = password[0];
	buf[11] = password[1];
	buf[12] = password[2];
	buf[13] = password[3];

	for(i = 0; i < len + 1; i++)
		check_sum += buf[i+6];

	buf[14] = (char)((check_sum & 0xff00) >> 8);
	buf[15] = (char)(check_sum & 0xff);

	write(fd, buf, 16);
	ret = ReadCode();

	return ret;
}

// 20.
/*
令芯片生成一个随机数并返回给上位机
*/
int GetRandomCode(void)
{
	char buf[SIZE_L];
	int ret;
	short len = 0x03;

	mkpacket(buf, SIGN_CMD, len, GETRANDOMCODE);
	buf[10] = 0;
	buf[11] = 0x18;

	write(fd, buf, 12);
	ret = ReadCode();

	return ret;
}

// 21.
/*
设置芯片地址
addr: 地址
*/
int SetChipAddr(unsigned int addr)
{
	char buf[SIZE_L];
	int ret, i;
	short len = 0x07;
	unsigned short check_sum = 0;

	mkpacket(buf, SIGN_CMD, len, SETCHIPADDR);
	buf[10] = (char)(addr >> 24);
	buf[11] = (char)(addr >> 16);
	buf[12] = (char)(addr >> 8);
	buf[13] = (char)(addr);

	for(i = 0; i < len + 1; i++)
		check_sum += buf[i+6];
	
	buf[14] = (char)(check_sum >> 8);
	buf[15] = (char)(check_sum);

	write(fd, buf, 16);
	ret = ReadCode();

	return ret;
}

// 22.
/*
读flash信息页
*/
int ReadINFpage(void)
{
	char buf[SIZE_L];
	int ret;
	short len = 0x03;

	mkpacket(buf, SIGN_CMD, len, READINFPAGE);
	buf[10] = 0x00;
	buf[11] = 0x1a;

	write(fd, buf, 12);
	ret = ReadCode();

	return ret;
}

// 23.
/*
端口控制
con: 控制码
     0  代表关闭端口
	 1  代表打开端口
*/
int PortControl(char con)
{
	char buf[SIZE_L];
	int ret, i;
	short len = 0x04;
	unsigned short check_sum = 0;

	mkpacket(buf, SIGN_CMD, len, PORTCONTROL);
	buf[10] = con;
	for(i = 0; i < len + 1; i++)
		check_sum += buf[i+6];
	buf[11] = (char)(check_sum >> 8);
	buf[12] = (char)(check_sum);

	write(fd, buf, 13);
	ret = ReadCode();

	return ret;
}

// 24.
/*
写记事本
pagenum : 页码 0--15
content : 用户信息
*/
int WriteNotepad(char pagenum, char *content)
{
	char buf[SIZE_H];
	int ret, i;
	short len = 36;
	unsigned short check_sum = 0;

	mkpacket(buf, SIGN_CMD, len, WRITENOTEPAD);
	buf[10] = pagenum;
	for(i = 0; i < 32; i++)
		buf[11+i] = content[i];

	for(i = 0; i < len + 1; i++)
		check_sum += buf[i+6];
	
	buf[43] = (char)(check_sum >> 8);
	buf[44] = (char)(check_sum);

	write(fd, buf, 45);
	ret = ReadCode();

	return ret;
}

// 25.
/*
读记事本
pagenum: 页码
*/
int ReadNotepad(char pagenum)
{
	char buf[SIZE_L];
	int ret, i;
	short len = 0x04;
	unsigned short check_sum = 0;

	mkpacket(buf, SIGN_CMD, len, READNOTEPAD);
	buf[10] = pagenum;
	for(i = 0; i < len + 1; i++)
		check_sum += buf[i+6];
	buf[11] = (char)(check_sum >> 8);
	buf[12] = (char)(check_sum);

	write(fd, buf, 13);
	ret = ReadCode();

	return ret;
}

// 26.
/*
上位机下载代码数据并写入flash
on : 升级模式
     0  仅进行信息页升级
	 1  完整升级
*/
int BurnCode(char on)
{
	char buf[SIZE_L];
	int ret, i;
	short len = 0x04;
	unsigned short check_sum = 0;

	mkpacket(buf, SIGN_CMD, len, BURNCODE);
	buf[10] = on;
	for(i = 0; i < len + 1; i++)
		check_sum += buf[i+6];
	buf[11] = (char)(check_sum >> 8);
	buf[12] = (char)(check_sum);

	write(fd, buf, 13);
	ret = ReadCode();

	return ret;
}

// 27.
/*
以charbuffer1 or charbuff2 中的特征文件高速搜索整个或者部分指纹库，如果成功搜索到则返回页码
bufid     : 缓冲区号
startpage : 起始页
pagenum   : 页数
*/
int HighSpeedSearch(char bufid, unsigned short startpage, unsigned short pagenum)
{
	char buf[SIZE_L];
	int ret, i;
	short len = 0x08;
	unsigned short check_sum = 0;

	mkpacket(buf, SIGN_CMD, len, HIGHSPEEDSEARCH);
	buf[10] = bufid;
	buf[11] = (char)(startpage >> 8);
	buf[12] = (char)(startpage);
	buf[13] = (char)(pagenum >> 8);
	buf[14] = (char)(pagenum);

	for(i = 0; i < len + 1; i++)
		check_sum += buf[i+6];

	buf[15] = (char)(check_sum >> 8);
	buf[16] = (char)(check_sum);

	write(fd, buf, 17);
	ret = ReadCode();

	return ret;
}

// 28.
/*
生成细化指纹图像
type: 图像类型
	0  二值化图像
	1  不含特征点的细化图像
	2  或其他：带有特征点的细化图像
*/
int GenBinImage(char type)
{
	char buf[SIZE_L];
	int ret, i;
	short len = 0x04;
	unsigned short check_sum = 0;

	mkpacket(buf, SIGN_CMD, len, GENBINIMAGE);
	buf[10] = type;
	for(i = 0; i < len + 1; i++)
		check_sum += buf[i+6];
	buf[11] = (char)(check_sum >> 8);
	buf[12] = (char)(check_sum);

	write(fd, buf, 13);
	ret = ReadCode();

	return ret;
}

// 29.
/*
读有效模板个数
*/
int ValidTempleteNum(void)
{
	char buf[SIZE_L];
	int  ret;
	short len = 0x03;

	mkpacket(buf, SIGN_CMD, len, VALIDTEMPLETENUM);
	buf[10] = 0;
	buf[11] = 0x21;
	
	write(fd, buf, 12);
	ret = ReadCode();

	return ret;
}

