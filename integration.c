/*
注册的时候两次确认指纹,上位机需要提醒用户，所以两种情况都做了函数
	1.两次确认指纹在一个函数内实现
	2.上位机分别调用两次指纹确认，第一次调FirstReg 然后提醒用户再次
	  验证指纹 再调第二次SecondReg
2013-6-21
加入线程服务
开启线程StartDetectService
停止线程StopDetectService
设置环境setJNIEnv
2013-7-12
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include "finger.h"

#include <jni.h>
#include <android/log.h>

#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, "wujiwang-jni", __VA_ARGS__))
#define LOGW(...) ((void)__android_log_print(ANDROID_LOG_WARN, "wujiwang-jni", __VA_ARGS__))
#define LOGE(...) ((void)__android_log_print(ANDROID_LOG_ERROR, "wujiwang-jni", __VA_ARGS__))


//全局变量
JavaVM *g_jvm = NULL;
jobject g_obj = NULL;
static int  finger_id;
static int  status = SERSTOP;
static int  fd_status = CLOSED;

// 线程函数
void *thread_fun(void* arg)
{
		JNIEnv *env;
		jclass cls;
		jmethodID mid;
		int ret;

//Attach主线程
		if((*g_jvm)->AttachCurrentThread(g_jvm, &env, NULL) != JNI_OK)
		{
				LOGE("%s: AttachCurrentThread() failed", __FUNCTION__);
				return NULL;
		}
		//找到对应的类
		cls = (*env)->GetObjectClass(env,g_obj);
		if(cls == NULL)
		{
				LOGE("FindClass() Error.....");
				goto error; 
		}
		//再获得类中的静态方法
		mid = (*env)->GetStaticMethodID(env, cls, "DetectFinger", "(I)V");
		if (mid == NULL) 
		{
				LOGE("GetMethodID() Error.....");
				goto error;  
		}
		//最后调用java中的静态方法

		while(1)
		{
			if(VERIFY == status)
			{
				ret = Identify();
				(*env)->CallStaticVoidMethod(env, cls, mid, ret);// 用回调的好处比较灵活java
														   // 可根据自己的需求处理界面
			}
			else if(SERPAUSE == status) // SERPAUSE 放前面好
			{
				LOGI("service is pausing...");
			}
			else if(SERSTOP == status)
			{
				LOGI("service is stoping ...");
				break;
			}
			usleep(50000);
		}

error:    
		//Detach主线程
		if((*g_jvm)->DetachCurrentThread(g_jvm) != JNI_OK)
		{
				LOGE("%s: DetachCurrentThread() failed", __FUNCTION__);
		}

		LOGI("service exit");
		pthread_exit(0);
		LOGI("service have exited");
		return NULL;
}
// 开启线程
static void start_thread(void)
{
		int i = 1;
		pthread_t pt;

		status = VERIFY;
		//创建子线程
		pthread_create(&pt, NULL, &thread_fun, (void *)i);
}
/*
返回当前的finger id即当前指纹库中的指纹个数
*/
int Java_wxp_jr_Fingerprint_FingerId(JNIEnv *env, jclass thiz)
{
	return finger_id;
}
/*
功能：清除指纹库
成功返回SUCCESS，失败返回错误码
*/
int Java_xwp_jr_Fingerprint_ClearStore(JNIEnv *env, jclass thiz)
{
	int ret;

	status = SERPAUSE;

	usleep(DELAYTIME);

	ret = Empty();
	if(ret == SUCCESS)
		finger_id = 0;
	
	status = VERIFY;

	return ret;
}
/*
功能：初始化指纹机
成功返回SUCCESS，失败返回错误码
*/
int  Java_xwp_jr_Fingerprint_InitFinger(JNIEnv *env, jclass thiz)
{
	int ret = ERR, i;
	
	if(CLOSED == fd_status)
	{
		ret = UartInit(); 
		if(ret < 0)
			goto EXIT;

		fd_status = OPENED;
		LOGI("打开设备成功 fd : %d", ret);
		LOGI("获取注册人数");
		ret = ValidTempleteNum();
	}

EXIT:
	switch(ret){
		case SUCCESS:
			finger_id = buf_data[0];
			LOGI("注册人数：%d", finger_id);
			break;

		case ERR:
			LOGI("设备已经打开 fd_status : %d", fd_status);
			break;
		default:
			LOGE("获取注册人数失败,错误码为: %d", ret);
			if(fd_status == OPENED)
			{
				UartClose();
				fd_status = CLOSED;
			}
			break;
	}
	
	return ret;
}

/*
功能：关闭串口, 停止服务
*/
void  Java_xwp_jr_Fingerprint_UartExit(JNIEnv *env, jclass thiz)
{
	status = SERSTOP;
	usleep(DELAYTIME);
	UartClose();
	fd_status = CLOSED;
}

//停止服务
void  Java_xwp_jr_Fingerprint_StopDetectService(JNIEnv *env, jobject obj)
{
	status = SERSTOP;
	usleep(DELAYTIME);
}


//由java调用以创建子线程
void Java_xwp_jr_Fingerprint_StartDetectService(JNIEnv* env, jobject obj)
{
	//创建子线程
	if(SERSTOP == status && OPENED == fd_status)  // 必须是文件打开状态并且线程只能开一次 因为线程之间会抢fd
	{
		start_thread();
		LOGI("线程开启");
	}
	else if(SERPAUSE == status)                   // 如果线程处于暂停状态可以通过本项开启
	{
		status = VERIFY;
		LOGI("线程重新开始");
	}
	else
	{
		LOGE("线程已经开启或设备未打开");
	}
}

/*
功能：两次确认指纹并注册指纹
成功返回SUCCESS，失败返回错误码
*/
int Java_xwp_jr_Fingerprint_FingerReg(JNIEnv *env, jclass thiz)
{
	int ret = ACK_NOFINGER;
	int count = 0;

	if(OPENED != fd_status)
	{
		LOGE("设备未打开");
		ret = -1;
		goto EXIT;
	}

	status = REGISTER;
	LOGI("开始注册指纹");
	usleep(DELAYTIME);

	while(ret == ACK_NOFINGER)
	{
		if(count > OVERTIME)
			break;
    	ret = GetImage();
		LOGI("获取第一次指纹中。。。。");
		count++;
		usleep(DELAYTIME);
	}
    if(ret != SUCCESS)
    {
		LOGE("第一次获取指纹失败");
        goto EXIT;
    }

    ret = GenChar(charbuffer1);
    if(ret != SUCCESS)
	{
		LOGE("第一次生成指纹库失败");
		goto EXIT;
	}

	ret = ACK_NOFINGER;
	count = 0;
	while(ret == ACK_NOFINGER)
	{
		if(count > OVERTIME)
			break;
    	ret = GetImage();
		LOGI("获取第二次指纹中。。。。");
		count++;
		usleep(DELAYTIME);
	}

    if(ret != SUCCESS)
    {   
		LOGE("第二次获取指纹失败");
        goto EXIT;
    }   

    ret = GenChar(charbuffer2);
    if(ret != SUCCESS)
    {
        LOGE("第二次生成指纹库失败");
        goto EXIT;
    }

    ret = RegModel();
    if(ret != SUCCESS)
    {
        LOGE("合并指纹库失败");
        goto EXIT;
    }

    ret = StoreChar(charbuffer2, finger_id);
    if(ret != SUCCESS)
    {
        LOGE("储存指纹库失败");
        goto EXIT;
    }
	finger_id++;
	LOGI("注册指纹总数：%d", finger_id);

EXIT:
	LOGI("注册指纹返回值ret : 0x%x", ret);
	status = VERIFY;

	return ret;
}



//由java调用来建立JNI环境
void Java_xwp_jr_Fingerprint_setJNIEnv( JNIEnv* env, jobject obj)
{
		(*env)->GetJavaVM(env,&g_jvm);
		g_obj = (*env)->NewGlobalRef(env,obj);
}

//动态调用自调用
int  JNI_OnLoad(JavaVM *vm, void *reserved)
{
		JNIEnv* env = NULL;
		jint result = -1;    

		// 获取jni版本  可省略
		if ((*vm)->GetEnv(vm, (void**)&env, JNI_VERSION_1_4) != JNI_OK) 
		{
				LOGE("GetEnv failed!");
				return result;
		}

		return JNI_VERSION_1_4;
}

/*
功能：第一次确认指纹
成功返回SUCCESS，失败返回错误码
*/
int Java_xwp_jr_Fingerprint_FirstReg(JNIEnv *env, jclass thiz)
{
	int ret = ACK_NOFINGER;
	int count = 0;

	if(OPENED == fd_status)
	{
		LOGE("设备未打开");
		ret = -1;
		goto EXIT;
	}

	status = REGISTER;
	LOGI("开始注册指纹1");
	usleep(DELAYTIME);
	while(ret == ACK_NOFINGER)
	{
		if(count > OVERTIME)
			break;

		LOGE("获取第一次指纹中。。。。");
    	ret = GetImage();
		count++;
		usleep(DELAYTIME);
	}
    if(ret != SUCCESS)
    {
        LOGE("第一次获取指纹失败。。");
        goto EXIT;
    }

    ret = GenChar(charbuffer1);
    if(ret != SUCCESS)
		LOGE("第一次生成指纹库失败");
EXIT:
	return ret;
}

/*
功能：第二次确认指纹并注册指纹
成功返回SUCCESS，失败返回错误码
*/
int Java_xwp_jr_Fingerprint_SecondReg(JNIEnv *env, jclass thiz)
{
	int ret = ACK_NOFINGER;
	int count = 0;

	if(OPENED != fd_status)
	{
		LOGE("设备未打开");
		goto EXIT;
	}

	while(ret == ACK_NOFINGER)
	{
		if(count > OVERTIME)
			break;

		LOGI("获取第二次指纹中。。。。");
    	ret = GetImage();
		count++;
		usleep(DELAYTIME);
	}

    if(ret != SUCCESS)
    {   
        LOGE("第二次获取指纹失败。。。");
        goto EXIT;
    }   

    ret = GenChar(charbuffer2);
    if(ret != SUCCESS)
    {
		LOGE("生成指纹特征库失败");
        goto EXIT;
    }

    ret = RegModel();
    if(ret != SUCCESS)
    {
		LOGE("合并指纹失败");
        goto EXIT;
    }

    ret = StoreChar(charbuffer2, finger_id);
    if(ret != SUCCESS)
    {
		LOGE("存储指纹失败");
        goto EXIT;
    }
	finger_id++;
	LOGI("注册指纹总数：%d", finger_id);

EXIT:
	status = VERIFY;

	return ret;
}

