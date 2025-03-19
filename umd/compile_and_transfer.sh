rm /home/benjamin/Desktop/NVDLA/transfers/nvdla_runtime

make clean

make TOOLCHAIN_PREFIX=aarch64-linux-gnu- ROOT=$(pwd)

cp ./out/runtime/nvdla_runtime/nvdla_runtime /home/benjamin/Desktop/NVDLA/transfers
