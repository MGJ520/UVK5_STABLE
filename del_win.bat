@echo off
setlocal

rem ����Ŀ��Ŀ¼
set "target_directory=./"  rem ��ǰĿ¼·��

rem ɾ�� .o �� .d �ļ�
for /r "%target_directory%" %%f in (*.o *.d) do (
    del "%%f"
)

echo Deletion (.o .d)File Sucess
endlocal

