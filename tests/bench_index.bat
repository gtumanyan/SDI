@echo off
rd /q /s logs
rd /q /s logs_bench

rd /q /s indexes
bin\SDI_R326.exe -license -preservecfg -drp_dir:..\drivers -nosnapshot -nostamp -verbose:320  -nogui
rename logs\log.txt log_326_a1.txt

rd /q /s indexes
bin\SDI_R326.exe -license -preservecfg -drp_dir:..\drivers -nosnapshot -nostamp -verbose:320  -nogui
rename logs\log.txt log_326_a2.txt

rd /q /s indexes
bin\SDI_R326.exe -license -preservecfg -drp_dir:..\drivers -nosnapshot -nostamp -verbose:320  -nogui
rename logs\log.txt log_326_a3.txt


bin\SDI_R326.exe -license -preservecfg -drp_dir:..\drivers -nosnapshot -nostamp -verbose:320  -nogui
rename logs\log.txt log_326_b1.txt

bin\SDI_R326.exe -license -preservecfg -drp_dir:..\drivers -nosnapshot -nostamp -verbose:320  -nogui
rename logs\log.txt log_326_b2.txt

bin\SDI_R326.exe -license -preservecfg -drp_dir:..\drivers -nosnapshot -nostamp -verbose:320  -nogui
rename logs\log.txt log_326_b3.txt


rd /q /s indexes
..\SDI_R.exe -license -preservecfg -drp_dir:..\drivers -nosnapshot -nostamp -verbose:320 -nogui
rename logs\log.txt log_R_a1.txt

rd /q /s indexes
..\SDI_R.exe -license -preservecfg -drp_dir:..\drivers -nosnapshot -nostamp -verbose:320 -nogui
rename logs\log.txt log_R_a2.txt

rd /q /s indexes
..\SDI_R.exe -license -preservecfg -drp_dir:..\drivers -nosnapshot -nostamp -verbose:320 -nogui
rename logs\log.txt log_R_a3.txt


..\SDI_R.exe -license -preservecfg -drp_dir:..\drivers -nosnapshot -nostamp -verbose:320 -nogui
rename logs\log.txt log_R_b1.txt

..\SDI_R.exe -license -preservecfg -drp_dir:..\drivers -nosnapshot -nostamp -verbose:320 -nogui
rename logs\log.txt log_R_b2.txt

..\SDI_R.exe -license -preservecfg -drp_dir:..\drivers -nosnapshot -nostamp -verbose:320 -nogui
rename logs\log.txt log_R_b3.txt

rename logs logs_bench
explorer logs_bench