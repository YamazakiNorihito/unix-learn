# MacでxV6を動かす手順

<https://github.com/mit-pdos/xv6-riscv.git> からpull

## 1. 必要なツールのインストール

### Homebrewのインストール（未インストールの場合）

```bash
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
```

### RISC-Vツールチェーンのインストール

```bash
brew tap riscv/riscv
brew install riscv-tools
```

これにより `riscv64-unknown-elf-gcc` などのRISC-Vコンパイラツールがインストールされます。

### QEMUのインストール

```bash
brew install qemu
```

xv6を動かすには、QEMU 7.2以上が必要です。

### インストールの確認

QEMUのバージョン確認：

```bash
qemu-system-riscv64 --version
```

RISC-Vツールチェーンの確認：

```bash
riscv64-unknown-elf-gcc --version
```

## 2. xv6のビルドと起動

xv6のプロジェクトディレクトリに移動して、以下のコマンドを実行します：

```bash
make qemu
```

このコマンドは以下を実行します：

- カーネルとユーザープログラムのビルド
- ファイルシステムイメージの作成
- QEMUエミュレータの起動
- xv6の実行

## 3. xv6での基本操作

xv6のシェルが起動したら、以下のコマンドが使用できます：

### ファイル操作

```bash
ls              # ファイル一覧の表示
cat README      # ファイルの内容表示
mkdir test      # ディレクトリ作成
rm filename     # ファイル削除
```

### その他のコマンド

```bash
echo hello      # 文字列の出力
grep pattern file  # ファイル内の文字列検索
wc filename     # 単語数、行数のカウント
```

### プログラムのテスト

```bash
usertests       # ユーザープログラムのテストスイート実行
```

## 4. xv6の終了方法

### xv6シェルからの終了

xv6シェルを終了するには：

```bash
exit
```

または `Ctrl + D` を押します。

### QEMUの終了方法

QEMU自体を終了するには、以下のキー操作を行います：

1. `Ctrl + A` を押す
2. 続けて `X` を押す

または、別のターミナルから以下のコマンドでQEMUプロセスを終了することもできます：

```bash
pkill qemu-system-riscv64
```

## トラブルシューティング

### ビルドエラーが発生する場合

クリーンビルド：

```bash
make clean
make qemu
```

### ツールチェーンが見つからない場合

パスの確認：

```bash
which riscv64-unknown-elf-gcc
which qemu-system-riscv64
```

Homebrewのパスが通っているか確認：

```bash
echo $PATH
```

## 参考情報

- xv6公式サイト: <https://pdos.csail.mit.edu/6.1810/>
- xv6 Book: <https://pdos.csail.mit.edu/6.828/2023/xv6/book-riscv-rev3.pdf>
- RISC-V仕様: <https://riscv.org/specifications/>
