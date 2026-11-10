#!/bin/bash
# 首次打开工程前生成本机配置；不覆盖已有配置或个人签名。
set -euo pipefail

project_dir=$(cd -- "$(dirname -- "$0")" && pwd)
config="$project_dir/build-profile.json5"
template="$project_dir/build-profile.example.json5"

if [[ -e "$config" || -L "$config" ]]; then
  echo "本机 build-profile.json5 已存在，保留原配置。"
else
  # 限制文件权限，并防止并发初始化覆盖刚生成的配置。
  (umask 077; set -o noclobber; cat "$template" > "$config")
  echo "已从公共模板生成 build-profile.json5，不含个人签名。"
fi

echo "请用兼容工程配置 6.1.1、SDK 6.1.1(24) 的 DevEco Studio 打开项目根目录。"
echo "需要安装到设备时，再在 DevEco Studio 配置自己的签名。"
if [[ -t 0 && -t 1 ]]; then
  read -r -p "按回车结束。" _
fi
