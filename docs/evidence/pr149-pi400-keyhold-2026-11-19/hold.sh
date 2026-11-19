I=/data/home/qnxuser/bin/rnqnx-inject5
L=/tmp/rnqnx-org.rnqnx.showcase.log
shot() { sudo -n screenshot -file=/tmp/pr149c-$1.bmp && sudo -n chmod 644 /tmp/pr149c-$1.bmp; echo shot_$1=$?; }
echo "log_lines_before=$(wc -l < $L)"
sudo -n $I move 600 300 move 606 304; shot A0
echo "CMD keyhold down 800 10"; sudo -n $I keyhold down 800 10; echo A_exit=$?
sleep 1; sudo -n $I move 600 300 move 606 304; sleep 1; shot A1
echo "CMD keyhold down 2500 (bg)"; (sudo -n $I keyhold down 2500; echo B_exit=$? >/tmp/pr149c-bexit) &
sleep 1.5; shot B_during
wait; cat /tmp/pr149c-bexit
sleep 1; sudo -n $I move 600 300 move 606 304; sleep 1; shot B1
sleep 2; shot B2
echo "log_lines_after=$(wc -l < $L)"; tail -n 5 $L
rm -f /tmp/pr149c-bexit
