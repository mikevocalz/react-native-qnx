I=/data/home/qnxuser/bin/rnqnx-inject5
shot() { sudo -n screenshot -file=/tmp/pr149c-$1.bmp && sudo -n chmod 644 /tmp/pr149c-$1.bmp; echo shot_$1=$?; }
sudo -n $I click 800 350 sleep 400 move 805 352; echo click_exit=$?
sudo -n $I key tab sleep 500 move 600 300; echo tab_exit=$?
sudo -n $I move 606 304; sleep 1; shot ex1
sudo -n $I key down sleep 500 move 600 300; echo down_exit=$?
sudo -n $I move 606 304; sleep 1; shot ex2
