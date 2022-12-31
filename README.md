J-Link EDU Reviver
==================

使用方法
--------
从右侧[releases](releases)页面下载最新[可执行文件包](https://github.com/banxian/EDUReviver/releases/download/v0.3.3-beta2/EDUReviver_b033_wdk.zip).
解压所有文件到目录中, 进入目录在命令行下执行想要的命令

EDUReViver -run blinky  
闪灯测试, 如果红灯闪烁12次, 说明设备能通过校验且网络通讯正常.  

EDUReViver -run revive  
去除多余的FlashDL之类的Features.  

EDUReViver -run swd  
打开电路板SWD调试接口.  

EDUReViver -run swd off  
关闭电路板SWD调试接口.  

EDUReViver -run to11  
转换V10/V12到V11. 重新插拔后, 打开jlink commander会自动刷新固件.  

EDUReViver -run to10  
转换V11/V12到V10. 重新插拔后, 打开jlink commander会自动刷新固件.  

EDUReViver -run to12  
转换V10/V11到V12. 重新插拔后, 打开jlink commander会自动刷新固件.  

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
download binary archive from [releases](releases) page. extract all files to same folder.
cd to folder then execute these commands in command prompt.

EDUReViver -run blinky  
this payload blinks red LED 12 times. this should used to test your usb and network connection is fine.  
  
EDUReViver -run revive  
this payload remove all extra features.  

EDUReViver -run swd  
this payload unlocking SWD debug port on PCB.  

EDUReViver -run swd off  
this payload locking SWD debug port on PCB again.  

EDUReViver -run to11  
this payload turns your V10/V12 to V11. unplug and replug usb cable, then flash new firmware in jlink commander.  

EDUReViver -run to10  
this payload turns your V11/V12 to V10. unplug and replug usb cable, then flash new firmware in jlink commander.  

EDUReViver -run to12  
this payload turns your V10/V11 to V12. unplug and replug usb cable, then flash new firmware in jlink commander.  

Disclaimer
----------
You can only use my utility for research propose, or repair your genuine jlink.