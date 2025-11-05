/*
 * 1.6 Exercises
 * 1. Write a program that uses UNIX system calls to "ping-pong" a byte between two processes
 *    over a pair of pipes, one for each direction. Measure the program's performance, in exchanges per second.
 *
 * 3. なぜやるのか？
 *
 * この課題は、OSのプロセス間通信 (IPC) と コンテキストスイッチ（切り替え） の基本を学ぶためです。
 *
 * pipe：プロセス間でデータを渡す仕組み
 *
 * fork：新しいプロセスを作る
 *
 * read/write：パイプを通して通信
 *
 * 測定：OSの性能（切り替えの速さ）を体感できる
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/wait.h>


int main(){
    int p1[2], p2[2];
    pipe(p1);
    pipe(p2);

    char byte = 'x';
    int n = 290940;

    struct timeval start, end;

    if(fork() == 0){
        close(p1[1]); // 親→子書き込み側
        close(p2[0]); // 子→親読み取り側
        for (int i = 0; i < n ; i++){
            read(p1[0], &byte, 1);
            write(p2[1], &byte, 1);
        }
        close(p1[0]);
        close(p2[1]);
        exit(0);
    }else{
        close(p1[0]); // 親→子読み取り側
        close(p2[1]); // 子→親書き込み側
        gettimeofday(&start, NULL);
        for (int i =0; i < n; i++){
            write(p1[1], &byte, 1);
            read(p2[0], &byte, 1);
        }
        close(p1[1]);
        close(p2[0]);
        gettimeofday(&end, NULL);

        wait(NULL);

        double elapsed = (end.tv_sec - start.tv_sec) + 
                        (end.tv_usec - start.tv_usec) / 1000000.0;
        
        printf("\n=== パフォーマンス測定結果 ===\n");
        printf("交換回数: %d 回\n", n);
        printf("経過時間（全体の時間）: %.6f 秒\n", elapsed);
        printf("スループット（1秒あたりの交換回数）: %.2f 回/秒\n", n / elapsed);
        printf("レイテンシ（1回あたりの平均時間）: %.6f マイクロ秒/回\n",
               (elapsed * 1000000) / n);
    }
}
