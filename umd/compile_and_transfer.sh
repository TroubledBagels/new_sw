destination="../../runtime_outputs/"

filename="${1:-nvdla_runtime}"

rm "${destination}${filename}"

make clean

make TOOLCHAIN_PREFIX=aarch64-linux-gnu- ROOT=$(pwd)

mkdir -p "${destination}"

mv ./out/runtime/nvdla_runtime/nvdla_runtime "${destination}${filename}"

echo "Successfully compiled and transferred ${filename} to ${destination}"
