branch=`git branch --show-current`
echo "user and password are both 'gnublocks'"
echo "git fetch; git reset --hard origin/$branch; git pull; ./buildVMLinux.sh; cp vm.linux.i386 /host; shutdown -h now" > /tmp/vm-script.sh
chmod +x /tmp/vm-script.sh
qemu-system-x86_64 -curses -m 512 -hda ubuntu16.04.img -virtfs local,path=/tmp,mount_tag=host0,security_model=passthrough,id=host0
mv /tmp/vm.linux.i386 .
rm /tmp/vm-script.sh
