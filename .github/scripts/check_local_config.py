"""检查 Git 跟踪文件，拦截本机配置和签名材料；不读取或打印个人配置。"""

import json
from pathlib import Path
import subprocess
import sys


SIGNING_SUFFIXES = {
    ".p12", ".pfx", ".jks", ".keystore", ".pem", ".key",
    ".cer", ".crt", ".csr", ".p7b", ".p8",
}


def forbidden(path: str) -> bool:
    return path in {"build-profile.json5", "local.properties"} or (
        Path(path).suffix.lower() in SIGNING_SUFFIXES
    )


def contains_signing(value) -> bool:
    if isinstance(value, dict):
        return any(
            key in {"signingConfig", "signingConfigs"} and bool(item)
            or contains_signing(item)
            for key, item in value.items()
        )
    if isinstance(value, list):
        return any(contains_signing(item) for item in value)
    return False


def main() -> int:
    root = Path(__file__).resolve().parents[2]
    tracked = subprocess.check_output(
        ["git", "ls-files", "-z"], cwd=root
    ).decode("utf-8").split("\0")
    blocked = [path for path in tracked if path and forbidden(path)]
    for path in blocked:
        print(f"禁止跟踪本机配置或签名材料：{path}", file=sys.stderr)
    template = json.loads((root / "build-profile.example.json5").read_text())
    signed = contains_signing(template)
    if signed:
        print("公共模板不得包含签名配置或签名引用。", file=sys.stderr)
    if blocked or signed:
        return 1
    print("本机配置、签名文件与公共模板检查通过。")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
