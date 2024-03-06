echo off
echo "Acquiring paths for vendor library copy"

set controlVendor=leap_control\vendor\
set vendorSource=\vendor\

set srcLeapCSharp=%~dp0%vendorSource%LeapCSharp\
set dstLeapCSharp=%~dp0%controlVendor%LeapCSharp\

set srcOvr=%~dp0%vendorSource%\openvr\
set dstOvr=%~dp0%controlVendor%\openvr\

echo "Source LeapCSharp: " %srcLeapCSharp%
echo "Destination LeapCSharp: " %dstLeapCSharp%
echo "Source OpenVR: " %srcOvr%
echo "Destination OpenVR: " %dstOvr%

::echo this file is important!> %srcLeapCSharp%\test.txt
robocopy %srcLeapCSharp% %dstLeapCSharp%

echo "Error level is %ERRORLEVEL%"
IF %ERRORLEVEL%==1 Exit 0
IF %ERRORLEVEL%==2 Exit 0