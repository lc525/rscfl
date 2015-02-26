objdump -d ${RSCFL_LINUX_VMLINUX} -j .init.text -j .exit.text | grep -oP '<\K.*(?=>:)' > $1
