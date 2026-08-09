#include <config.h>
#include <video_manager.h>
#include <string.h>
static PT_VideoOpr ptVideoOprHeader = NULL;
/**********************************************************************
 * 函数名称： RegisterVideoOpr
 * 功能描述： 注册"摄像头模块", 所谓摄像头模块就是获取图像帧的模块
 * 输入参数： ptVideoOpr - 一个结构体,内含"获取图像帧"的操作函数
 * 输出参数： 无
 * 返 回 值： 0 - 成功, 其他值 - 失败
 * *********************************************************************/
int RegisterVideoOpr(PT_VideoOpr ptVideoOpr)//尾插法注册操作函数的结构体
{    PT_VideoOpr PTtmp;   // 临时指针, 用于遍历链表 
     if(ptVideoOprHeader == NULL){
        ptVideoOprHeader = ptVideoOpr;
        return 0;
     }
     else{
        PTtmp = ptVideoOprHeader;
        while(PTtmp -> ptNext != NULL){
            PTtmp = PTtmp -> ptNext;
        }
        PTtmp -> ptNext = ptVideoOpr;
        ptVideoOpr -> ptNext = NULL;
     }
    return 0;
}
/**********************************************************************
 * 函数名称： ShowVideoOpr
 * 功能描述： 显示所有注册的摄像头模块信息
 * 输入参数： 无
 * 输出参数： 无
 * 返 回 值： 无
 * *********************************************************************/
void ShowVideoOpr(void){
    PT_VideoOpr PTtmp = ptVideoOprHeader;
    while(PTtmp){
        printf("VideoOprName is %s\n",PTtmp->name);
        PTtmp = PTtmp->ptNext;
    }
}
/**********************************************************************
 * 函数名称： getVideoOpr
 * 功能描述： 根据摄像头模块名称获取对应的操作函数结构体指针
 * 输入参数： pcName - 摄像头模块名称字符串
 * 输出参数： 无
 * 返 回 值： PT_VideoOpr - 对应的操作函数结构体指针, 如果未找到则返回NULL
 * *********************************************************************/
PT_VideoOpr getVideoOpr(char *pcName){
    PT_VideoOpr PTtmp = ptVideoOprHeader;
    while(PTtmp){
        if(strcmp(PTtmp->name,pcName) == 0){
            return PTtmp;
        }
        PTtmp = PTtmp->ptNext;
    }
    return NULL;
}

/**********************************************************************
 * 函数名称： addVideoOpr
 * 功能描述： 添加一个视频操作函数结构体
 * 输入参数： ptVideoOpr - 对应的操作函数结构体指针
 * 输出参数： 无
 * 返 回 值： 无
 ********************************************************************/
int VideoInit(void)
{
	int iError;

    iError = V4l2Init();

	return iError;
}
//ai加
/**********************************************************************
 * 函数名称： VideoDeviceInit
 * 功能描述： 初始化摄像头设备: 选择v4l2驱动, 绑定操作接口, 并调用其InitDevice
 * 输入参数： pcDevName - 摄像头设备节点, 如/dev/video0
 *           ptVideoDevice - 待初始化的设备结构体
 * 返 回 值： 0 - 成功, 其他值 - 失败
 * *********************************************************************/
int VideoDeviceInit(char *pcDevName, PT_VideoDevice ptVideoDevice)
{
    PT_VideoOpr ptVideoOpr = getVideoOpr("v4l2");
    if(NULL == ptVideoOpr)
    {
        DBG_PRINTF("can not get v4l2 video opr\n");
        return -1;
    }

    memset(ptVideoDevice, 0, sizeof(T_VideoDevice));
    ptVideoDevice->ptOPr = ptVideoOpr;

    return ptVideoOpr->InitDevice(pcDevName, ptVideoDevice);
}
