connect -url tcp:127.0.0.1:3121
source C:/Users/Lello/Desktop/AES/Lab2/image_uart_Z7/image_uart_Z7.sdk/design_1_wrapper_hw_platform_0/ps7_init.tcl
targets -set -nocase -filter {name =~"APU*" && jtag_cable_name =~ "Digilent Zybo Z7 210351B0FC9BA"} -index 0
loadhw -hw C:/Users/Lello/Desktop/AES/Lab2/image_uart_Z7/image_uart_Z7.sdk/design_1_wrapper_hw_platform_0/system.hdf -mem-ranges [list {0x40000000 0xbfffffff}]
configparams force-mem-access 1
targets -set -nocase -filter {name =~"APU*" && jtag_cable_name =~ "Digilent Zybo Z7 210351B0FC9BA"} -index 0
stop
ps7_init
ps7_post_config
targets -set -nocase -filter {name =~ "ARM*#0" && jtag_cable_name =~ "Digilent Zybo Z7 210351B0FC9BA"} -index 0
rst -processor
targets -set -nocase -filter {name =~ "ARM*#0" && jtag_cable_name =~ "Digilent Zybo Z7 210351B0FC9BA"} -index 0
dow C:/Users/Lello/Desktop/AES/Lab2/image_uart_Z7/image_uart_Z7.sdk/lab2_hist_equalization/Debug/lab2_hist_equalization.elf
configparams force-mem-access 0
targets -set -nocase -filter {name =~ "ARM*#0" && jtag_cable_name =~ "Digilent Zybo Z7 210351B0FC9BA"} -index 0
con
