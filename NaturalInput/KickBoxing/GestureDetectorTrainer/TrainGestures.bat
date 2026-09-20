REM Generate Labeled Examples
REM -------------------------

call GenerateLabeledExamples.exe Punch_Right "..\Data" "..\Data"
call GenerateLabeledExamples.exe Punch_Left "..\Data" "..\Data"
call GenerateLabeledExamples.exe Kick_Right "..\Data" "..\Data"
call GenerateLabeledExamples.exe Kick_Left "..\Data" "..\Data"

REM Train Gestures - RETAIL BUILD - long training times, extremely accurate
REM --------------

REM call TrainGesture.exe Punch_Right "..\Data" "..\Data\Gestures\PunchRight.gesture" 0.98
REM call TrainGesture.exe Punch_Left "..\Data" "..\Data\Gestures\PunchLeft.gesture" 0.98
REM call TrainGesture.exe Kick_Right "..\Data" "..\Data\Gestures\KickRight.gesture" 0.98
REM call TrainGesture.exe Kick_Left "..\Data" "..\Data\Gestures\KickLeft.gesture" 0.98

REM Train Gestures - RELEASE BUILD - medium training times, very accurate
REM --------------

call TrainGesture.exe Punch_Right "..\Data" "..\Data\Gestures\PunchRight.gesture"
call TrainGesture.exe Punch_Left "..\Data" "..\Data\Gestures\PunchLeft.gesture"
call TrainGesture.exe Kick_Right "..\Data" "..\Data\Gestures\KickRight.gesture"
call TrainGesture.exe Kick_Left "..\Data" "..\Data\Gestures\KickLeft.gesture"

REM Train Gestures - QUICK EXPERIMENTS - fast training times, accurate
REM -----------------

REM call TrainGesture.exe Punch_Right "..\Data" "..\Data\Gestures\PunchRight.gesture" 0.8
REM call TrainGesture.exe Punch_Left "..\Data" "..\Data\Gestures\PunchLeft.gesture" 0.8
REM call TrainGesture.exe Kick_Right "..\Data" "..\Data\Gestures\KickRight.gesture" 0.8
REM call TrainGesture.exe Kick_Left "..\Data" "..\Data\Gestures\KickLeft.gesture" 0.8
