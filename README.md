J-Link EDU Reviver
==================

使用方法
--------
从右侧[releases](https://github.com/banxian/EDUReviver/releases)页面下载最新[可执行文件包](https://github.com/banxian/EDUReviver/releases/download/v0.3.9-beta/EDUReviver_bin_b039.zip).
解压所有文件到目录中, 进入目录在命令行下执行想要的命令

`EDUReViver -run blinky`  
闪灯测试, 如果红灯闪烁12次, 说明设备能通过校验且网络通讯正常.  

`EDUReViver -run revive`  
去除多余的FlashDL之类的Features.  

`EDUReViver -run swd`  
启用JLink电路板上的调试接口, 可以用来救砖.  

`EDUReViver -run swd off`  
禁用JLink电路板上的调试接口.  

`EDUReViver -run to11`  
转换V10/V12到V11. 成功转换后, 打开J-Link Commander会自动刷新固件.  

`EDUReViver -run to10`  
转换V11/V12到V10. 成功转换后, 打开J-Link Commander会自动刷新固件. 原装V12无法降级.  

`EDUReViver -run to12`  
转换V10/V11到V12. 成功转换后, 打开J-Link Commander会自动刷新固件. 注意因为没有V12真机, 此工具转换的V12 Bootloader不原装.  

支持设备
--------
J-Link V10/V11 所有版本, 目前为止所有固件. 支持错误添加了Features的正版. 但不能是山寨版. 我加了一些在线检测避免滥用.   
一次只能支持一个设备, 支持配置为WinUSB驱动, 如果出现识别错误, 请在JLinkConfig配置程序中切换到Segger驱动.

免责声明
--------
此工具仅供研究和学习使用, 请勿将其用于商业用途. 请勿在山寨版上使用, 有可能导致工具下架.


Introduce
---------
This utility support all version of JLinkV10/V11. support genuine jlink brick by bad features.   
doesn't support illegal clones. some check applied on my utility to prevent running on clones.   
turn off winusb mode in JlinkConfig if you meet usb communicate problems.

Usage
--------
download binary archive from [releases](https://github.com/banxian/EDUReviver/releases) page. extract all files to same folder.
cd to folder then execute these commands in command prompt.

`EDUReViver -run blinky`  
this payload blinks red LED 12 times. this should used to test your usb and network connection is fine.  
  
`EDUReViver -run revive`  
this payload remove all extra features.  

`EDUReViver -run swd`  
this payload unlocking SWD debugging on JLink PCB.  

`EDUReViver -run swd off`  
this payload locking SWD debugging on JLink PCB again.  

`EDUReViver -run to11`  
this payload turns your V10/fakeV12 to V11. after payload sucessed, please launch J-Link Commander to flash firmware.  

`EDUReViver -run to10`  
this payload turns your V11/fakeV12 to V10. after payload sucessed, please launch J-Link Commander to flash firmware.  

`EDUReViver -run to12`  
this payload turns your V10/V11 to fakeV12. after payload sucessed, please launch J-Link Commander to flash firmware.  

Disclaimer
----------
You can only use my utility for research propose, or repair your genuine jlink.