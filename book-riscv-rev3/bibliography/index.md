Bibliography

[1] Linux common vulnerabilities and exposures (CVEs). <https://cve.mitre.org/cgi-bin/cvekey.cgi?keyword=linux>
> Linuxカーネルに関する既知の脆弱性（CVE）のデータベース。セキュリティ上の欠陥がどのように分類・追跡されるかを示す。

[2] The RISC-V instruction set manual Volume I: unprivileged ISA. <https://github.com/riscv/riscv-isa-manual/releases/download/Ratified-IMAFDQC/riscv-spec-20191213.pdf>, 2019.
> RISC-Vの非特権（ユーザモード）命令セットの公式仕様書。整数演算、浮動小数点、アトミック命令などを定義している。

[3] The RISC-V instruction set manual Volume II: privileged architecture. <https://github.com/riscv/riscv-isa-manual/releases/download/Priv-v1.12/riscv-privileged-20211203.pdf>, 2021.
> RISC-Vの特権アーキテクチャの公式仕様書。マシンモード・スーパーバイザモードの制御レジスタ、割り込み、ページテーブルなどOS実装に必要な機構を定義している。

[4] Hans-J Boehm. Threads cannot be implemented as a library. ACM PLDI Conference, 2005.
> スレッドをライブラリだけで正しく実装することは不可能であり、言語仕様やコンパイラレベルでのメモリモデルの定義が必要であることを論じた論文。

[5] Edsger Dijkstra. Cooperating sequential processes. <https://www.cs.utexas.edu/users/EWD/transcriptions/EWD01xx/EWD123.html>, 1965.
> 並行プログラミングの基礎を築いた古典的論文。セマフォの概念を導入し、相互排除や同期の問題を体系的に論じた。

[6] Maurice Herlihy and Nir Shavit. The Art of Multiprocessor Programming, Revised Reprint. 2012.
> マルチプロセッサ環境での並行プログラミングの教科書。ロック、ロックフリーデータ構造、線形化可能性などを網羅している。

[7] Brian W. Kernighan. The C Programming Language. Prentice Hall Professional Technical Reference, 2nd edition, 1988.
> 通称「K&R」。C言語の設計者自身による定番の入門書・リファレンス。xv6のコードを読む上で前提となるC言語の知識を提供する。

[8] Gerwin Klein, Kevin Elphinstone, Gernot Heiser, June Andronick, David Cock, Philip Derrin, Dhammika Elkaduwe, Kai Engelhardt, Rafal Kolanski, Michael Norrish, Thomas Sewell, Harvey Tuch, and Simon Winwood. Sel4: Formal verification of an OS kernel. In Proceedings of the ACM SIGOPS 22nd Symposium on Operating Systems Principles, page 207–220, 2009.
> マイクロカーネルseL4の形式検証に成功した画期的な論文。OSカーネルが仕様通りに動作することを数学的に証明した。

[9] Donald Knuth. Fundamental Algorithms. The Art of Computer Programming. (Second ed.), volume 1. 1997.
> コンピュータサイエンスの金字塔的著作。基本的なデータ構造とアルゴリズムを数学的に厳密に解説している。

[10] L Lamport. A new solution of dijkstra's concurrent programming problem. Communications of the ACM, 1974.
> Dijkstraの相互排除問題に対する新しい解法（Bakeryアルゴリズム）を提案した論文。共有メモリを用いたロックの理論的基盤を提供する。

[11] John Lions. Commentary on UNIX 6th Edition. Peer to Peer Communications, 2000.
> UNIX V6のソースコードに対する逐行解説書。xv6の設計はこのUNIX V6に強く影響を受けている。

[12] Paul E. Mckenney, Silas Boyd-wickizer, and Jonathan Walpole. RCU usage in the linux kernel: One decade later, 2013.
> Linuxカーネルで広く使われるRCU（Read-Copy-Update）同期機構の10年間の発展と実用例をまとめた論文。ロックなしで読み取り側の性能を最大化する手法。

[13] Martin Michael and Daniel Durich. The NS16550A: UART design and application considerations. <http://bitsavers.trailing-edge.com/components/national/_appNotes/AN-0491.pdf>, 1987.
> NS16550A UARTチップの設計と応用に関する技術文書。xv6のコンソール入出力で使用されるシリアル通信ハードウェアの仕様を解説している。

[14] Aleph One. Smashing the stack for fun and profit. <http://phrack.org/issues/49/14.html#article>
> バッファオーバーフロー攻撃の原理を解説した有名な記事。スタック上のリターンアドレスを書き換えて任意コードを実行する手法を示す。

[15] David Patterson and Andrew Waterman. The RISC-V Reader: an open architecture Atlas. Strawberry Canyon, 2017.
> RISC-Vアーキテクチャの入門書。命令セットの設計思想と各命令の解説を簡潔にまとめている。

[16] Dave Presotto, Rob Pike, Ken Thompson, and Howard Trickey. Plan 9, a distributed system. In In Proceedings of the Spring 1991 EurOpen Conference, pages 43–50, 1991.
> Bell Labsで開発された分散OS「Plan 9」の設計論文。「すべてをファイルとして扱う」というUNIXの思想をネットワーク環境に拡張した。

[17] Dennis M. Ritchie and Ken Thompson. The UNIX time-sharing system. Commun. ACM, 17(7):365–375, July 1974.
> UNIXの設計者自身によるUNIXの原論文。ファイルシステム、プロセス、シェルなどの基本設計を解説しており、xv6の思想的原点。
