此工具支持JLinkV10/V11到目前为止所有固件版本. 支持错误添加了Features的正版. 唯一要求, 不能是山寨版. 我加了一些在线检测避免滥用.
一次只能支持一个设备, 支持设置为WinUSB驱动的新版固件, 如果出现识别错误, 请在JLinkConfig配置程序中切换到Segger驱动.

使用方法:
解压所有文件到同一目录中, 在命令行下执行

EDUReViver -run blinky
闪灯测试, 如果红灯闪烁12次, 说明设备能通过校验且网络通讯正常.

EDUReViver -run revive
去除多余的FlashDL之类的Features.

EDUReViver -run swd
打开SWD调试接口.

EDUReViver -run to11
转换V10到V11. 重新插拔后, 打开jlink commander会自动刷新固件.

EDUReViver -run to10
转换V11到V10. 重新插拔后, 打开jlink commander会自动刷新固件.

此工具仅供研究和学习使用, 请勿将其用于商业用途.