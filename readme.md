# myDevice

## 说明
- **myDevice**是一个可以用来管理设备的框架工具；
- 目前实现设备的**FOTA**能力，可以用来帮助开发人员或者测试等相关人员，快速对设备进行升级；
- 整个方案包括**设备端**和**PC端上位机**；

## 设备端使用说明
设备端提供了**源码**或者**ESP32库文件**；
这里介绍库文件的使用；
>如果你的平台是非ESP32,可以直接使用源码进行编译使用，如有使用问题可以通过微信公众号(**晓谈物联**)与我联系;

设备端只需要调用`myDevice` API 即可启动此服务,其他参数见demo设置；


## 上位机工具

## 设备端资源使用

在`ESP32`下通过`size-components`命令查看的资源使用情况如下
```sh
 Archive File DRAM .data & .bss & other   IRAM   D/IRAM Flash code & rodata   Total
libmyDevice.a         40     28       0      0        0       3026     1893    4987
```