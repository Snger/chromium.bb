:: Copyright (c) 2011 The Native Client Authors. All rights reserved.
:: Use of this source code is governed by a BSD-style license that can be
:: found in the LICENSE file.

@echo off
setlocal
set HERMETIC_CYGWIN=hermetic_cygwin_1_7_9-0_1
if exist "%~dp0..\cygwin\%HERMETIC_CYGWIN%.installed" goto :skip_cygwin_install
if not exist "%~dp0..\cygwin" goto :dont_remove_cygwin
attrib -H "%~dp0..\cygwin\.svn"
move "%~dp0..\cygwin\.svn" "%~dp0..\cygwin-.svn"
rmdir /s /q "%~dp0..\cygwin"
if errorlevel 1 goto :rmdir_fail
mkdir "%~dp0..\cygwin"
move "%~dp0..\cygwin-.svn" "%~dp0..\cygwin\.svn"
attrib +H "%~dp0..\cygwin\.svn"
:dont_remove_cygwin
cscript //nologo //e:jscript "%~dp0get_file.js" http://commondatastorage.googleapis.com/nativeclient-mirror/nacl/cygwin_mirror/%HERMETIC_CYGWIN%.exe "%~dp0%HERMETIC_CYGWIN%.exe"
if errorlevel 1 goto :download_fail
:download_success
start /WAIT %~dp0%HERMETIC_CYGWIN%.exe /DEVEL /S /D=%~dp0..\cygwin
if errorlevel 1 goto :install_fail
set CYGWIN=nodosfilewarning
"%~dp0..\cygwin\bin\touch" "%~dp0..\cygwin\%HERMETIC_CYGWIN%.installed"
if errorlevel 1 goto :install_fail
del /f /q "%~dp0%HERMETIC_CYGWIN%.exe"
:skip_cygwin_install
endlocal
set "PATH=%~dp0..\cygwin\bin;%PATH%"
goto :end
:rmdir_fail
endlocal
echo Failed to remove old version of cygwin
set ERRORLEVEL=1
goto :end
:download_fail
c:\cygwin\bin\wget http://commondatastorage.googleapis.com/nativeclient-mirror/nacl/cygwin_mirror/%HERMETIC_CYGWIN%.exe -O "%~dp0%HERMETIC_CYGWIN%.exe"
if errorlevel 1 goto :wget_fail
goto download_success
:wget_fail
c:\cygwin\bin\wget http://commondatastorage.googleapis.com/nativeclient-mirror/nacl/cygwin_mirror/%HERMETIC_CYGWIN%.exe -O "%~dp0%HERMETIC_CYGWIN%.exe"
if errorlevel 1 goto :cygwin_wget_fail
goto download_success
:cygwin_wget_fail
endlocal
echo Failed to download cygwin
set ERRORLEVEL=1
goto :end
:install_fail
endlocal
echo Failed to install cygwin
set ERRORLEVEL=1
goto :end
:end
