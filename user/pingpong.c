#include "kernel/types.h"
#include "user.h"

int main(int argc, char* argv[]){
    if (argc != 1){
        printf("Pingpong needs none argument!\n");
        exit(-1);
    }

    int pc[2];
    int pf[2];
    if (pipe(pc) < 0 || pipe(pf) < 0) {  // 确保管道成功创建
        printf("Error creating pipe\n");
        exit(-1);
    }

    if (fork() == 0) {
        // 子进程
        close(pf[1]);  // 关闭写入端
        close(pc[0]);  // 关闭读取端

        int father_id;
        int children_id = getpid();
        read(pf[0], &father_id, sizeof(father_id));

        close(pf[0]);  // 关闭读取端
        printf("%d: received ping from pid %d\n", children_id, father_id);


        write(pc[1], &children_id, sizeof(children_id));
        close(pc[1]);  // 关闭子进程写入端

    } else {
        // 父进程
        close(pc[1]);  // 关闭写入端
        close(pf[0]);  // 关闭读取端
        int father_id = getpid();
        write(pf[1], &father_id, sizeof(father_id));
        close(pf[1]);  // 关闭写入端，确保数据已经发送

        int children_id;
        read(pc[0], &children_id, sizeof(children_id));
        close(pc[0]);  // 关闭父进程的读取端

        printf("%d: received pong from pid %d\n", father_id, children_id);

        wait(0);  // 等待子进程完成
    }

    exit(0);
}
