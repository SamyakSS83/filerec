cd /home/threesamyak/filerec && mkdir -p test_output && dd if=/dev/sda1 of=test_image.img bs=10M count=1
cd /home/threesamyak/filerec && cat tests/test_files/normal.jpeg tests/test_files/ok.pdf >> test_image.img
cd /home/threesamyak/filerec && ls -la test_image.img
cd /home/threesamyak/filerec && ./build/FileRecoveryTool -v --signature-only test_image.img test_output
cd /home/threesamyak/filerec && timeout 30 ./build/FileRecoveryTool -v --signature-only test_image.img test_output 2>&1