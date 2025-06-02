cd /home/threesamyak/filerec && mkdir -p test_output && dd if=/dev/zero of=test_image.img bs=1M count=1
cd /home/threesamyak/filerec && cat tests/test_files/normal.jpg tests/test_files/ok.pdf >> test_image.img
cd /home/threesamyak/filerec && ls -la test_image.img
cd /home/threesamyak/filerec && ./build/FileRecoveryTool -v --signature-only test_image.img test_output
cd /home/threesamyak/filerec && timeout 30 ./build/FileRecoveryTool -v --signature-only test_image.img test_output 2>&1