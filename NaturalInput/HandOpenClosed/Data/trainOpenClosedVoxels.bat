"HOCTrainer.exe" -v 0 128 -T "traininglist.txt" "traininglistTest.txt" opt_v0_128.hoc > opt_v0_128.hoc.txt
"HOCTrainer.exe" -v 128 256 -T "traininglist.txt" "traininglistTest.txt" opt_v128_256.hoc > opt_v128_256.hoc.txt
"HOCTrainer.exe" -p 1 0.3 -v 256 4096 -T "traininglist.txt" "traininglistTest.txt" opt_v256_4096.hoc > opt_v256_4096.hoc.txt

