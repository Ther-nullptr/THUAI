@echo off
start F:\utf-8Previewv1.0\Previewv1.0\runServer1.cmd
choice /t 1 /d y /n >nul
start F:\utf-8Previewv1.0\Previewv1.0\runAgent.cmd
choice /t 1 /d y /n >nul
start F:\utf-8Previewv1.0\Previewv1.0\CAPI\x64\Debug\CAPI.exe -I 127.0.0.1 -P 7777 -p 0 -t 0 -d -w  
choice /t 2 /d y /n >nul
start F:\utf-8Previewv1.0\Previewv1.0\runClient.cmd
