@echo off
start F:\utf-8Previewv1.0\Previewv1.0\runServer1.cmd %运行server,这里要修改为自己的绝对路径%
choice /t 1 /d y /n >nul %暂停1s%
start F:\utf-8Previewv1.0\Previewv1.0\runAgent.cmd %运行agent,这里要修改为自己的绝对路径%
choice /t 1 /d y /n >nul %暂停1s%
start F:\utf-8Previewv1.0\Previewv1.0\CAPI\x64\Debug\CAPI.exe -I 127.0.0.1 -P 7777 -p 0 -t 0 -d -w  %以相应的命令行参数(可以更改)执行exe文件,注意要先通过编译%
choice /t 2 /d y /n >nul %暂停2s%
start F:\utf-8Previewv1.0\Previewv1.0\runClient.cmd %运行client,这里要修改为自己的绝对路径%
